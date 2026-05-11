# API Reference

## Module: `bionetgen`

### `bionetgen.load(path) → BioNetGenModel`

Load a BNGL model file.

**Parameters:**
- `path` (str | Path) — Path to the `.bngl` file.

**Returns:** `BioNetGenModel`

**Raises:** `_bionetgen_cpp.ParseError` if the file has syntax errors.

---

### `bionetgen.run(path, method="ode", t_end=100.0, n_steps=100, **kwargs) → SimResult`

Load a model and run simulation in one call.

**Parameters:**
- `path` (str | Path) — Path to the `.bngl` file.
- `method` (str) — `"ode"`, `"ssa"`, or `"nf"`.
- `t_end` (float) — End time.
- `n_steps` (int) — Number of output steps.
- `**kwargs` — Passed to `BioNetGenModel.simulate()`.

**Returns:** `SimResult`

---

## Class: `BioNetGenModel`

Created by `bionetgen.load()`. Wraps the C++ model and provides simulation methods.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `parameters` | `list[Parameter]` | Model parameters |
| `molecule_types` | `list[MoleculeType]` | Molecule type definitions |
| `seed_species` | `list[SeedSpecies]` | Initial species and concentrations |
| `observables` | `list[Observable]` | Observable definitions |
| `reaction_rules` | `list[ReactionRule]` | Reaction rule definitions |
| `functions` | `list[Function]` | User-defined functions |
| `compartments` | `list[Compartment]` | Compartment definitions |
| `actions` | `list[Action]` | Action block entries |
| `name` | `str` | Model name |

### Methods

#### `set_parameter(name: str, value: float) → None`

Set a parameter value by name.

**Raises:** `KeyError` if parameter not found.

---

#### `generate_network(max_iter: int = 100) → GeneratedNetwork`

Generate the reaction network by iterative rule application.

**Parameters:**
- `max_iter` — Maximum iterations for network generation.

**Returns:** `GeneratedNetwork` with `.num_species` and `.num_reactions`.

---

#### `simulate(method="ode", t_end=100.0, n_steps=100, ...) → SimResult`

Run a simulation.

**Parameters:**
- `method` (str) — `"ode"`, `"ssa"`, or `"nf"`.
- `t_end` (float) — End time.
- `n_steps` (int) — Number of output time steps.
- `t_start` (float) — Start time (default 0.0).
- `rtol` (float) — Relative tolerance, ODE only (default 1e-8).
- `atol` (float) — Absolute tolerance, ODE only (default 1e-8).
- `seed` (int) — Random seed for SSA/NF (default 0 = system).
- `verbose` (bool) — Print progress (default False).

**Returns:** `SimResult`

---

#### `execute(verbose: bool = False) → None`

Execute all actions defined in the model's action block.

---

#### `write_xml(path: str) → None`

Export model to BioNetGen XML format.

#### `write_bngl(path: str) → None`

Export model back to BNGL format.

#### `write_net(path: str) → None`

Export generated network to `.net` format. Generates network if needed.

#### `write_sbml(path: str) → None`

Export to SBML Level 2 Version 3. Generates network if needed.

#### `write_matlab(path: str) → None`

Export to MATLAB ODE script. Generates network if needed.

#### `write_latex(path: str) → None`

Export to LaTeX document. Generates network if needed.

---

## Class: `SimResult`

Container for simulation output.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `time` | `np.ndarray` | 1D array of time points |
| `observables` | `dict[str, np.ndarray]` | Observable name → values array |
| `concentrations` | `np.ndarray \| None` | 2D array (steps × species), ODE/SSA only |
| `n_steps` | `int` | Number of time points |
| `observable_names` | `list[str]` | List of observable names |

### Methods

#### `to_dataframe() → pandas.DataFrame`

Convert to DataFrame with time and observable columns. Requires pandas.

---

#### `plot(observables=None, **kwargs) → tuple[Figure, Axes]`

Plot observable time courses.

**Parameters:**
- `observables` (list[str] | None) — Subset to plot; None = all.
- `**kwargs` — Passed to `matplotlib.pyplot.plot()`.

---

## Low-Level C++ Bindings: `_bionetgen_cpp`

For advanced usage, the C++ bindings can be used directly:

```python
import _bionetgen_cpp as _cpp

model = _cpp.parse_file("model.bngl")
network = _cpp.generate_network(model, max_iter=100)
result = _cpp.simulate_ode(model, network, t_end=100, n_steps=200)

# result is a dict with numpy arrays
print(result["time"])
print(result["observables"])
```

See `_bionetgen_cpp.pyi` for full type annotations.

---

## Exceptions

### `_bionetgen_cpp.ParseError`

Raised when:
- A BNGL file cannot be opened
- The file contains syntax errors
- AST construction fails

```python
try:
    model = bionetgen.load("bad_model.bngl")
except _bionetgen_cpp.ParseError as e:
    print(f"Parse failed: {e}")
```
