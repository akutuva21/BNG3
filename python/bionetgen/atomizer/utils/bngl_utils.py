import re

bioqual = [
    "BQB_IS",
    "BQB_HAS_PART",
    "BQB_IS_PART_OF",
    "BQB_IS_VERSION_OF",
    "BQB_HAS_VERSION",
    "BQB_IS_HOMOLOG_TO",
    "BQB_IS_DESCRIBED_BY",
    "BQB_IS_ENCODED_BY",
    "BQB_ENCODES",
    "BQB_OCCURS_IN",
    "BQB_HAS_PROPERTY",
    "BQB_IS_PROPERTY_OF",
    "BQB_HAS_TAXON",
    "BQB_UNKNOWN",
]

modqual = [
    "BQM_IS",
    "BQM_IS_DESCRIBED_BY",
    "BQM_IS_DERIVED_FROM",
    "BQM_IS_INSTANCE_OF",
    "BQM_HAS_INSTANCE",
    "BQM_UNKNOWN",
]

annotationHeader = {"BQB": "bqbiol", "BQM": "bmbiol"}


def standardizeName(name):
    """
    Remove stuff not used by bngl
    """
    name2 = name

    sbml2BnglTranslationDict = {
        "^": "",
        "'": "",
        "*": "m",
        " ": "_",
        "#": "sh",
        ":": "_",
        "α": "a",
        "β": "b",
        "γ": "g",
        " ": "",
        "+": "pl",
        "/": "_",
        ":": "_",
        "-": "_",
        ".": "_",
        "?": "unkn",
        ",": "_",
        "(": "",
        ")": "",
        "[": "",
        "]": "",
        ">": "_",
        "<": "_",
    }

    for element in sbml2BnglTranslationDict:
        name = name.replace(element, sbml2BnglTranslationDict[element])
    name = re.sub(r"[\W]", "", name)
    return name
