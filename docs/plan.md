# Adaptive GPU Points-to Analysis — Development Plan

## 1. Project overview

**Working title:** Adaptive GPU Constraint Propagation for Andersen-Style Points-to Analysis

**Goal:** Build a correct, flow-insensitive, inclusion-based points-to solver and investigate whether **jointly adapting points-to set representation (B)** and **GPU task scheduling (D)** improves throughput, memory consumption, and robustness across diverse constraint graphs.

**Target implementation:** C++20 + HIP on AMD GPU, with CPU reference and optimized multicore CPU baselines. Use LLVM/SVF as an *optional frontend* after the solver and workload format are stable. The project does not depend on rustc.

**Primary research question:** Can representation-aware GPU scheduling outperform fixed-representation and fixed-scheduling GPU solvers across real-world workloads, and when does it outperform a strong multicore CPU baseline?

### Success criteria

1. Exact points-to results matching a trusted reference for all supported constraints.
2. Reproducible real-program and synthetic benchmarks, with published workload characteristics.
3. Independent ablations for representation adaptation, scheduling adaptation, and their combination.
4. Measurements of solver-only and end-to-end costs, including conversion, transfers, initialization, and memory use.
5. A defensible conclusion even if GPU speedup is absent; do not promise a speedup in advance.

### Non-goals

- Full flow-sensitive, context-sensitive, or field-sensitive pointer analysis.
- Building a compiler frontend from scratch.
- Replacing LLVM/SVF's production analyses.
- Claiming novelty solely from implementing known GPU propagation or hybrid sets.

## 2. Scope and semantic contract

Use an abstract, flow-insensitive Andersen constraint language over pointer variables `V` and abstract objects `O`:

| Constraint | Input notation | Meaning |
| --- | --- | --- |
| Address-of | `addr(p, o)` | `o ∈ PT(p)` |
| Copy | `copy(q, p)` | `PT(q) ⊆ PT(p)` |
| Load | `load(q, p)` | For each `o ∈ PT(q)`, add inclusion edge `o → p` |
| Store | `store(p, q)` | For each `o ∈ PT(p)`, add inclusion edge `q → o` |

For load/store, abstract objects must be modeled as addressable locations capable of holding pointer values; define a uniform node-ID namespace or an explicit object-to-location mapping. This distinction is essential for correct indirect constraints.

**Solver invariant:** Starting from address-of seeds, repeatedly apply monotone constraints to compute the **least fixed point**. New load/store discoveries add inclusion edges; adding an edge must immediately account for any points-to elements already present at its source. Deduplicate edges and points-to insertions. Use set equality, not iteration counts, for correctness.

### Exclusions and assumptions

- Initially assume no casts, pointer arithmetic, function pointers, unknown external calls, or field offsets.
- Treat each allocation/global as a distinct abstract object.
- Later, frontend extraction must document how unsupported IR constructs are conservatively modeled or rejected. Never silently omit constraints while claiming sound analysis of a whole program.
- Define whether the analysis is whole-program and how external/library code is represented.

## 3. Architecture

```text
C/C++ program / LLVM IR
          │
     optional SVF adapter
          │
          ▼
    Constraint workload ───► Validator + statistics
          │
          ├──► Sequential CPU reference ───► Expected PT sets
          ├──► Multicore CPU solver
          └──► HIP solver
                   ├── fixed dense baseline
                   ├── adaptive representation (B)
                   ├── adaptive scheduling (D)
                   └── joint adaptive solver (B+D)
          │
          ▼
     Differential checker + benchmark harness
```

**Separation of concerns:** The frontend produces semantic constraints. Each solver constructs its own internal representation and uses the *same input file*. Keep input parsing and graph preprocessing outside solver-only timings; include them in end-to-end timings.

## 4. Workload format

Implement a versioned binary format and an easy-to-debug text format. Include:

- Version, endianness, number of variable nodes and abstract objects.
- Object-to-location-node mapping and address-of seed pairs.
- Copy, load, and store constraints as pairs of node IDs.
- Optional labels for debugging and provenance (source function/module).
- Checksums and bounds validation; deterministic node numbering.

For the first iteration, a simple text representation is sufficient:

```text
pta-v1
vars 4
objects 2
location 0 2
location 1 3
addr 0 0
copy 0 1
load 0 1
store 1 0
```

This is **illustrative**: freeze exact indexing, node kinds, and load/store operand conventions in a schema document and tests before relying on it. Convert to a compact binary/CSR-like representation after correctness is established.

**Statistics:** node count, edge count, constraint-type distribution, degree histograms, SCC sizes, points-to cardinality histograms, density/chunk occupancy, and number of dynamic edges discovered.

## 5. Baselines

### 5.1 Sequential reference

- C++20 deterministic worklist solver with an uncomplicated set representation (e.g., `std::vector<uint64_t>` dense bitsets for manageable workloads, or sorted sets when memory demands).
- Support all four constraints and dynamic edge creation.
- Verify fixed-point stability with an additional full constraint scan.
- Unit tests for cycles, empty sets, duplicate edges, load/store chains, self-loops, and propagation after a late-added edge.
- Use this implementation as the correctness oracle, not as the only performance comparator.

### 5.2 Optimized multicore CPU baseline

- Implement a parallel CPU solver (C++ threads/TBB/OpenMP; choose one and document it).
- Batch independent graphs first; optionally parallelize one large graph with synchronized worklists.
- Report thread count, scaling, and synchronization overhead.
- Include a strong sequential optimized baseline (not just an intentionally naive reference).

### 5.3 Fixed GPU baseline

- HIP kernel(s) using dense bitsets and a fixed execution mapping.
- Start with a correctness-first bulk-synchronous iteration, host-controlled convergence, and a deterministic worklist or changed flag.
- Implement load/store dynamic edge generation correctly, initially with a simpler staged approach.
- Compare single-graph execution against batching many independent graphs.
- Capture kernel launches, transfers, allocation costs, and peak VRAM.

## 6. Optimization B — Adaptive points-to representations

Implement and benchmark:

1. **Sparse sorted arrays** for tiny sets. Favor compact storage and efficient iteration; account for insertion/merge cost.
2. **Chunked bitsets** for clustered IDs. Use fixed-size chunks with a directory of present chunks; evaluate chunk widths (e.g., 256/512/1024 bits).
3. **Dense bitsets** for large sets with efficient wordwise OR and atomic OR where needed.

### Conversion and concurrency

- Define a common API: `contains`, `insert`, `union_delta`, `iterate`, `cardinality`, and `memory_bytes`.
- Maintain per-set metadata (representation tag, size, capacity, version).
- Avoid unsafe concurrent conversion: begin with **epoch-boundary conversion** after synchronization; later investigate concurrent conversion only if profiling justifies it.
- Avoid repeated thrashing using hysteresis (different promote/demote thresholds).
- Record conversion frequency, bytes copied, allocation overhead, and time spent converting.
- Explore object-ID reordering/clustering as a **separate controlled experiment** because it changes chunked-bitset effectiveness.

### Policies to compare

- Fixed sparse / fixed chunked / fixed dense.
- Cardinality-only threshold policy.
- Occupancy- and memory-aware policy.
- Measured-cost policy using recent union and update statistics.

## 7. Optimization D — Adaptive GPU scheduling

Classify propagation work using representation, source delta size, destination size, and graph fan-out. Start with separate device queues or buckets:

| Class | Candidate execution mapping | Typical work |
| --- | --- | --- |
| Tiny | One lane/task | Small sparse insertion or copy |
| Medium | Sub-wavefront or wavefront/task | Medium sparse/chunked unions |
| Large | Workgroup/task | Dense wordwise OR |
| Very large | Split into tiled tasks | High-cardinality sets or high fan-out |

AMD hardware wavefront width and supported HIP execution behavior must be queried, not assumed. Benchmark mapping choices on the actual GPU.

### Scheduling variants

- One fixed mapping for all tasks.
- Static bucket thresholds from microbenchmarks.
- Online cost model using set sizes, representation, delta size, fan-out, and measured cost.
- Joint representation-aware scheduling policy.

### Correctness and termination

- Prefer epoch/bulk-synchronous processing initially.
- Ensure dynamic edges and new set elements are not lost between epochs.
- Use atomic insertion/OR or exclusive ownership where required.
- Implement a reliable global termination test: no pending deltas, no newly discovered edges, no queued tasks, and no in-flight updates.
- Persistent kernels and asynchronous work stealing are stretch goals, not prerequisites.

## 8. Joint B+D design

The central hypothesis is that optimal execution mapping depends on *both* graph work and set representation. For example, a sparse-list merge and dense bitwise OR with the same cardinality may require different mappings.

Compare policies:

- `FixedDense + FixedSchedule` (GPU baseline)
- `AdaptiveRep + FixedSchedule` (B)
- `FixedDense + AdaptiveSchedule` (D)
- `AdaptiveRep + AdaptiveSchedule` (B+D)
- Optional `AdaptiveRep + RepUnawareSchedule` to isolate the value of **joint** decisions.

Do not tune and evaluate on the same workload corpus. Use held-out programs or graph families to test generalization.

## 9. Frontend and realistic workloads

### Phase 1: Synthetic and hand-written constraints

- Generate graphs with controlled size, copy-edge fan-out, SCC structure, load/store density, and set-cardinality distribution.
- Include adversarial cases: one giant set, many tiny sets, hub nodes, long chains, cycles, and dynamic-edge explosions.
- Seed and record all randomness.

### Phase 2: LLVM/SVF integration

- Pin a tested LLVM/SVF revision and document its build dependencies.
- Investigate SVF's constraint graph and points-to APIs; write a small adapter that exports **semantic constraints**, not SVF's computed answers.
- Validate adapter output on tiny C/C++ programs where expected points-to sets are hand-checkable.
- Select representative open-source programs and record the exact source revisions and build commands.
- Cross-check against SVF where the analysis configuration and abstraction match; otherwise explain semantic differences.
- Do not assume SVF's graph is a drop-in implementation of the simplified format; verify treatment of field sensitivity, external functions, and indirect calls.

## 10. Correctness strategy

1. **Unit tests:** Each constraint type and composition, cycles, duplicate inputs, empty graphs, self-copy, and late edge insertion.
2. **Differential tests:** Compare exact points-to sets for every node across reference, CPU parallel, and all GPU variants.
3. **Randomized tests:** Generate many small graphs; run all solvers and shrink failures to minimal cases.
4. **Metamorphic tests:** Reordering/duplicating constraints must not change results; adding constraints cannot remove points-to relationships.
5. **Fixed-point check:** Independently verify that every constraint is satisfied by the final output.
6. **Concurrency stress:** Repeat under different task orders, queue sizes, and thread/workgroup counts.
7. **Memory safety:** Validate indices/capacities and use available HIP debugging/profiling tools.

All benchmark results must come from versions that pass correctness checks.

## 11. Benchmark methodology

### Measurements

- Solver-only wall time (preprocessed workload already resident where applicable).
- End-to-end wall time (parse, preprocessing, allocations, host-device transfers, solve, result retrieval).
- Peak host RAM and GPU VRAM.
- GPU kernel time, number of launches, queue occupancy, idle time, contention, and representation-conversion overhead.
- Number of propagation tasks, successful insertions, dynamic edges, and fixed-point epochs.
- Speedup relative to optimized sequential CPU, multicore CPU, and fixed GPU baseline.

### Controls

- Warm up GPU and CPU; perform repeated trials and report median and dispersion.
- Pin compiler/toolchain, GPU driver/ROCm version, clocks/power settings where feasible, and CPU thread counts.
- Avoid comparing GPU batched throughput to CPU single-graph latency without clearly labeling the metric.
- Compare both **latency** for one graph and **throughput** for many graphs.
- Use held-out workloads for final claims; document any failed runs or out-of-memory cases.
- Provide a performance breakdown to identify whether transfers, allocations, scheduling, or actual propagation dominate.

### Hypotheses

- H1: Hybrid representations reduce memory use and/or solver time on workloads with heterogeneous set sizes.
- H2: Workload-aware scheduling reduces idle time and improves GPU throughput on skewed task distributions.
- H3: Joint B+D outperforms either adaptation alone on at least some held-out workload classes.
- H4: GPU benefit depends strongly on graph size, batching, and load/store density; there may be regimes where multicore CPU remains superior.

## 12. Repository layout

```text
adaptive-gpu-pta/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── docs/
│   ├── DEVELOPMENT_PLAN.md
│   ├── constraint-format.md
│   ├── semantics.md
│   ├── architecture.md
│   └── experiments.md
├── include/pta/
│   ├── constraints.hpp
│   ├── solver.hpp
│   └── result.hpp
├── src/
│   ├── common/          # parser, validator, graph statistics
│   ├── cpu/             # reference and parallel baselines
│   ├── hip/             # kernels, sets, queues, scheduler
│   └── frontend/        # optional SVF adapter
├── tests/
│   ├── unit/
│   ├── differential/
│   └── fuzz/
├── benchmarks/
│   ├── generators/
│   ├── workloads/
│   ├── scripts/
│   └── results/
└── third_party/         # pinned external dependencies
```

## 13. Timeline (8 weeks; adjust to course deadline)

| Week | Deliverables | Exit criterion |
| --- | --- | --- |
| 1 | Freeze semantics/schema; toolchain + HIP smoke test; test generator | Simple constraints parse; HIP kernel runs |
| 2 | Sequential reference + tests + verifier | Randomized small-graph tests pass |
| 3 | Fixed dense HIP solver + CPU performance baseline | Exact agreement on all supported constraints |
| 4 | Profiling, batch mode, synthetic workload suite | Bottlenecks identified; reproducible baseline |
| 5 | Sparse/chunked/dense representations + conversion | B ablation passes correctness and memory checks |
| 6 | Task bucketing + adaptive scheduling | D ablation passes correctness and profiling |
| 7 | Joint B+D, tuning on training workloads; optional SVF adapter | All variants comparable on held-out cases |
| 8 | Final evaluation, report, plots, demo, code cleanup | Reproducible figures and defensible conclusions |

**Schedule risk:** If SVF integration or load/store dynamic-edge handling is unexpectedly difficult, prioritize correctness of the complete four-constraint solver over adding more optimization variants.

## 14. Milestone acceptance checklist

### M0 — Environment
- [ ] HIP smoke test runs on target AMD GPU.
- [ ] Exact toolchain/ROCm versions recorded.
- [ ] CMake/CTest and benchmark harness operational.

### M1 — Correct reference
- [ ] Four constraint types implemented.
- [ ] Dynamic edges and late propagation verified.
- [ ] Randomized tests and independent fixed-point verifier pass.

### M2 — Comparable baselines
- [ ] Optimized sequential and multicore CPU baselines measured.
- [ ] Dense HIP solver passes differential tests.
- [ ] Latency, throughput, and end-to-end measurements separated.

### M3 — Representation adaptation (B)
- [ ] Sparse, chunked, dense sets implemented and tested.
- [ ] Conversion is race-free and measured.
- [ ] At least two adaptive policies compared.

### M4 — Scheduling adaptation (D)
- [ ] Tiny/medium/large task classes implemented.
- [ ] Fixed vs adaptive scheduling compared.
- [ ] GPU scheduling overhead profiled.

### M5 — Final research result
- [ ] Full ablation matrix completed.
- [ ] Held-out workload evaluation completed.
- [ ] Memory and runtime tradeoffs analyzed.
- [ ] Negative results and limitations reported.
- [ ] Scripts reproduce figures and tables.

## 15. Risks and fallback plans

| Risk | Mitigation |
| --- | --- |
| HIP unsupported on development GPU/OS | Verify on day one; arrange compatible GPU/cluster or focus on multicore CPU + GPU-capable remote host |
| GPU loses to optimized CPU | Study crossover regimes, batching, and memory behavior; report negative result rigorously |
| GPU dynamic graph updates too complex | Stage dynamic-edge discovery and propagation in synchronized epochs |
| Adaptive conversions are costly | Use hysteresis, epoch boundaries, and conversion budget; compare against fixed policies |
| Memory fragmentation or allocator contention | Preallocate arenas/pools and track allocation costs |
| SVF integration consumes schedule | Use stable constraint format and synthetic/hand-written real examples first; make adapter optional |
| Workload tuning overfits | Separate tuning and evaluation corpora |
| Limited novelty | Frame contribution as a measured joint-policy design; conduct focused related-work review before claiming novelty |

## 16. Final report structure

1. Motivation and research questions.
2. Background: Andersen analysis, dynamic constraint edges, GPU architecture.
3. Related work: GPU points-to analysis, hybrid sets, graph scheduling, CPU parallel solvers.
4. Semantics, input format, and correctness requirements.
5. Baseline implementations.
6. Adaptive representation design (B).
7. Adaptive scheduling design (D) and joint policy.
8. Experimental methodology and workloads.
9. Results: correctness, ablations, latency, throughput, memory, bottleneck breakdown.
10. Threats to validity, limitations, and future work.

## 17. Immediate next steps

1. Verify HIP compilation/execution on the target GPU; save system and toolchain versions.
2. Freeze the node/object model and constraint file schema in `docs/semantics.md`.
3. Implement the reference solver and independent fixed-point verifier.
4. Write a randomized graph generator and differential test harness.
5. Implement the fixed dense HIP baseline before adaptive policies.
6. Profile real task distributions, then decide representation thresholds and scheduling buckets from measurements rather than assumptions.

**First implementation gate:** Do not start B or D until the reference and fixed GPU solvers agree on address-of, copy, load, and store constraints, including dynamically discovered edges.
