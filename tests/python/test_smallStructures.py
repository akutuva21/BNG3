import pytest
from bionetgen.atomizer.utils.smallStructures import readFromString
from pyparsing.exceptions import ParseException


def test_readFromString_basic():
    # Test molecule without components
    sp = readFromString("A()")
    assert len(sp.molecules) == 1
    assert sp.molecules[0].name == "A"
    assert len(sp.molecules[0].components) == 0

    sp2 = readFromString("A")
    assert len(sp2.molecules) == 1
    assert sp2.molecules[0].name == "A"
    assert len(sp2.molecules[0].components) == 0


def test_readFromString_components():
    # Test molecule with a simple component
    sp = readFromString("A(b)")
    mol = sp.molecules[0]
    assert len(mol.components) == 1
    assert mol.components[0].name == "b"
    assert mol.components[0].states == []
    assert mol.components[0].bonds == []


def test_readFromString_states_and_bonds():
    # Test component with state
    sp = readFromString("A(b~P)")
    comp = sp.molecules[0].components[0]
    assert comp.name == "b"
    assert comp.states == ["P"]
    assert comp.bonds == []

    # Test component with bond
    sp2 = readFromString("A(b!1)")
    comp2 = sp2.molecules[0].components[0]
    assert comp2.name == "b"
    assert comp2.states == []
    assert comp2.bonds == ["1"]

    # Test component with state and bond
    sp3 = readFromString("A(b~P!1)")
    comp3 = sp3.molecules[0].components[0]
    assert comp3.name == "b"
    assert comp3.states == ["P"]
    assert comp3.bonds == ["1"]


def test_readFromString_multiple_components():
    # Test molecule with multiple components
    sp = readFromString("A(b!1,c~U)")
    mol = sp.molecules[0]
    assert len(mol.components) == 2
    assert mol.components[0].name == "b"
    assert mol.components[0].bonds == ["1"]
    assert mol.components[1].name == "c"
    assert mol.components[1].states == ["U"]


def test_readFromString_multiple_molecules():
    # Test species with multiple molecules
    sp = readFromString("A(b!1).B(a!1)")
    assert len(sp.molecules) == 2
    assert sp.molecules[0].name == "A"
    assert sp.molecules[0].components[0].name == "b"
    assert sp.molecules[0].components[0].bonds == ["1"]
    assert sp.molecules[1].name == "B"
    assert sp.molecules[1].components[0].name == "a"
    assert sp.molecules[1].components[0].bonds == ["1"]


def test_readFromString_invalid():
    # Test invalid inputs
    with pytest.raises(ParseException):
        readFromString("!@#")

    with pytest.raises(ParseException):
        readFromString("()")
