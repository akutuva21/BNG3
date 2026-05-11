# User Guide

## Loading Models

The primary entry point is `bionetgen.load()`, which parses a BNGL file and returns a model object:

```python
import bionetgen

model = bionetgen.load("path/to/model.bngl")
```

You can also parse BNGL from a string:

```python
import _bionetgen_cpp as _cpp

model_obj = _cpp.parse_string("""
begin model
begin parameters
    k 0.1
end parameters
begin molecule types
    A(b)
end molecule types
begin seed species
    A(b) 100
end seed species
begin observables
    Molecules Atot A()
end observables
begin reaction rules
    A(b) -> 0 k
end reaction rules
end model
""")
```

## Inspecting Model Components

```python
model = bionetgen.load("model.bngl")

# Parameters
for p in model.parameters:
    print(f"{p.name} = {p.expression}")

# Molecule types
for mt in model.molecule_types:
    print(f"{mt.name}({', '.join(c.name for c in mt.components)})")

# Reaction rules
for rule in model.reaction_rules:
    print(f"  {rule.label} (bidirectional={rule.is_bidirectional})")

# Observables
for obs in model.observables:
    print(f"  {obs.name} [{obs.type}]: {obs.patterns}")

# Seed species
for sp in model.seed_species:
    print(f"  {sp.pattern} = {sp.amount}")
```

## Modifying Parameters

```python
model = bionetgen.load("model.bngl")
model.set_parameter("k_on", 2.0)
model.set_parameter("k_off", 0.05)
```

## Network Generation

Before running ODE or SSA simulations, the reaction network must be generated (enumerating all reachable species and reactions):

```python
network = model.generate_network(max_iter=100)
print(f"Generated {network.num_species} species, {network.num_reactions} reactions")
```

This step is automatic when you call `model.simulate()` with `method="ode"` or `method="ssa"`.

## Simulation

### ODE (Deterministic)

```python
result = model.simulate(
    method="ode",
    t_end=100.0,
    n_steps=200,
    rtol=1e-8,
    atol=1e-8,
)
```

### SSA (Stochastic)

```python
result = model.simulate(
    method="ssa",
    t_end=100.0,
    n_steps=200,
    seed=42,
)
```

### Network-Free (NFSim)

For models with large or infinite state spaces where network enumeration is infeasible:

```python
result = model.simulate(
    method="nf",
    t_end=100.0,
    n_steps=200,
    seed=42,
)
```

Network-free simulation does not require `generate_network()` — it operates directly on the rule definitions.

## Working with Results

### SimResult Object

```python
result = model.simulate(method="ode", t_end=100)

# Time array
print(result.time)          # numpy array, shape (n_steps+1,)

# Observable values
print(result.observables)   # dict: name → numpy array

# Species concentrations (ODE/SSA only)
print(result.concentrations)  # numpy array, shape (n_steps+1, n_species)

# Number of time points
print(result.n_steps)

# Observable names
print(result.observable_names)
```

### Plotting

```python
# Plot all observables
fig, ax = result.plot()

# Plot specific observables
fig, ax = result.plot(observables=["A_free", "AB_complex"])
```

### Export to DataFrame

```python
df = result.to_dataframe()
print(df.head())
#    time     Afree        AB
# 0   0.0  100.000  0.000000
# 1   0.5   95.123  4.876543
# ...
```

## Executing Model Actions

If your BNGL file has an `actions` block (e.g., `generate_network`, `simulate`, `writeXML`), you can execute all actions:

```python
model = bionetgen.load("model.bngl")
model.execute(verbose=True)
```

This is equivalent to running `bng_cpp model.bngl` from the command line.

## Exporting Models

```python
model = bionetgen.load("model.bngl")

# Export to various formats
model.write_xml("model.xml")
model.write_bngl("model_copy.bngl")

# These require network generation first:
model.write_net("model.net")
model.write_sbml("model.sbml")
model.write_matlab("model.m")
model.write_latex("model.tex")
```

## Parameter Scans

```python
import numpy as np
import bionetgen

model = bionetgen.load("model.bngl")
k_values = np.logspace(-2, 2, 20)
final_obs = []

for k in k_values:
    model.set_parameter("k_on", k)
    model._network = None  # force re-generation
    result = model.simulate(method="ode", t_end=100)
    final_obs.append(result.observables["AB"][-1])

import matplotlib.pyplot as plt
plt.semilogx(k_values, final_obs)
plt.xlabel("k_on")
plt.ylabel("Final [AB]")
plt.show()
```

## One-Line Simulation

For quick simulations without needing the model object:

```python
result = bionetgen.run("model.bngl", method="ode", t_end=100, n_steps=200)
result.plot()
```

## Legacy Compatibility

To fall back to the Perl-based BNG2.pl engine (requires Perl installed):

```bash
export BIONETGEN_USE_PERL=1
```

This is only needed if you encounter a model that the C++ engine doesn't yet handle correctly.
