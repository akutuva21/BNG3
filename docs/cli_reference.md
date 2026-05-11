# CLI Reference

The `bionetgen` command-line tool provides access to all core functionality.

## Commands

### `bionetgen run`

Run a BNGL model simulation.

```bash
bionetgen run MODEL [OPTIONS]
```

**Arguments:**
- `MODEL` — Path to the `.bngl` file.

**Options:**
- `-m, --method [ode|ssa|nf]` — Simulation method (default: ode).
- `-t, --t-end FLOAT` — End time (default: 100.0).
- `-n, --n-steps INT` — Number of output steps (default: 100).
- `-o, --output PATH` — Output file path (TSV format).
- `-v, --verbose` — Verbose output.

**Examples:**
```bash
bionetgen run model.bngl --method ode --t-end 1000 --n-steps 500
bionetgen run model.bngl -m ssa -o results.tsv
bionetgen run model.bngl -m nf -t 50 -v
```

---

### `bionetgen execute`

Execute all actions defined in a BNGL model file (equivalent to running `bng_cpp model.bngl`).

```bash
bionetgen execute MODEL [OPTIONS]
```

**Options:**
- `-v, --verbose` — Verbose output.

**Examples:**
```bash
bionetgen execute model.bngl
bionetgen execute model.bngl --verbose
```

---

### `bionetgen check`

Parse a BNGL file and report model statistics or syntax errors.

```bash
bionetgen check MODEL
```

**Examples:**
```bash
$ bionetgen check model.bngl
OK: /path/to/model.bngl
  Parameters:      12
  Molecule types:  4
  Seed species:    6
  Reaction rules:  8
  Observables:     5
  Actions:         2
```

```bash
$ bionetgen check bad_model.bngl
ERROR: BNGL syntax errors in bad_model.bngl: 3 error(s)
```

---

### `bionetgen export`

Export a model to another format.

```bash
bionetgen export MODEL -f FORMAT -o OUTPUT
```

**Options:**
- `-f, --format [xml|net|bngl|sbml|matlab|latex]` — Output format.
- `-o, --output PATH` — Output file path (required).

**Examples:**
```bash
bionetgen export model.bngl -f sbml -o model.xml
bionetgen export model.bngl -f matlab -o model.m
bionetgen export model.bngl -f net -o model.net
bionetgen export model.bngl -f latex -o model.tex
```

---

## Environment Variables

| Variable | Description |
|----------|-------------|
| `BIONETGEN_USE_PERL` | Set to `1` to use legacy Perl BNG2.pl engine |
| `BNGPATH` | Path to legacy BNG2.pl installation (only for Perl fallback) |
