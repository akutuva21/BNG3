# BioNetGen 3

Unified rule-based modeling platform combining BioNetGen (C++ engine), NFSim (network-free simulator), and PyBioNetGen (Python interface) into a single package.

## Installation

```bash
pip install bionetgen
```

### From source

```bash
git clone <repo-url>
cd BNG3
pip install -e .
```

## Quick Start

```python
import bionetgen

# Load and simulate a model
model = bionetgen.load("model.bngl")
result = model.simulate(method="ode", t_end=100, n_steps=200)

# Plot results
result.plot()

# Access raw data
print(result.time)
print(result.observables)
df = result.to_dataframe()
```

## CLI

```bash
# Run a simulation
bionetgen run model.bngl --method ode --t-end 100

# Check syntax
bionetgen check model.bngl

# Export to other formats
bionetgen export model.bngl --format sbml --output model.xml
```

## Architecture

- **C++ Backend** (`cpp/`): ANTLR4 parser, AST, network generation, ODE/SSA simulation, NFSim engine
- **Python Frontend** (`python/bionetgen/`): High-level API, CLI, plotting, atomizer
- **pybind11 Bindings** (`cpp/bindings/`): Zero-copy bridge between C++ and Python

## Simulation Methods

- `"ode"` — Deterministic ODE integration (CVODE/SUNDIALS)
- `"ssa"` — Stochastic simulation algorithm
- `"nf"` — Network-free simulation (NFSim engine)

## Building C++ Only

```bash
cmake -B build -DBUILD_PYTHON_BINDINGS=OFF -DBUILD_CLI=ON
cmake --build build
./build/cpp/bng_cpp model.bngl
```

## License

MIT
