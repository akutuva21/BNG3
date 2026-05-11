import pytest
from unittest.mock import MagicMock, patch
from bionetgen.atomizer.atomizer.moleculeCreation import createBindingRBM


@patch("bionetgen.atomizer.atomizer.moleculeCreation.getComplexationComponents2")
def test_create_binding_rbm_keyerror(mock_get_complexation, capsys):
    """
    Test the KeyError error path in createBindingRBM where the translator
    cannot find the molecule name.
    """
    # Create inputs for createBindingRBM
    element = ("mock_element",)

    # An empty translator will trigger KeyError when accessed with molecule[0].name
    translator = {}

    # Needs to match the element
    dependencyGraph = {"mock_element": [["UnknownMolecule"]]}

    # Create mock molecules that will be returned by getComplexationComponents2
    mol1 = MagicMock()
    mol1.name = "UnknownMolecule"
    mol1.components = []

    mol2 = MagicMock()
    mol2.name = "Mol2"
    mol2.components = []

    # When createBindingRBM calls getComplexationComponents2, return a pair of molecules
    mock_get_complexation.return_value = [[mol1, mol2]]

    database = MagicMock()
    database.partialUserLabelDictionary = {}
    database.constructedSpecies = []

    # The code we want to test:
    # try:
    #     if newComponent1.name not in [
    #         x.name for x in translator[molecule[0].name].molecules[0].components
    #     ]: ...
    # except KeyError as e:
    #     print("The translator doesn't know the molecule: {}".format(molecule[0].name))
    #     raise e

    # The exception IS re-raised at line 812 (`raise e`), so we DO expect the function to crash!
    with pytest.raises(KeyError) as excinfo:
        createBindingRBM(
            element=element,
            translator=translator,
            dependencyGraph=dependencyGraph,
            bioGridFlag=False,
            pathwaycommonsFlag=False,
            parser=None,
            database=database,
        )

    # Also verify the printed output
    captured = capsys.readouterr()
    assert "The translator doesn't know the molecule: UnknownMolecule" in captured.out
