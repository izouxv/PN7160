idf_component_register(
    SRCS
        "NdefLibrary/src/P2P_NDEF.c"
        "NdefLibrary/src/RW_NDEF.c"
        "NdefLibrary/src/RW_NDEF_MIFARE.c"
        "NdefLibrary/src/RW_NDEF_T1T.c"
        "NdefLibrary/src/RW_NDEF_T2T.c"
        "NdefLibrary/src/RW_NDEF_T3T.c"
        "NdefLibrary/src/RW_NDEF_T4T.c"
        "NdefLibrary/src/RW_NDEF_T5T.c"
        "NdefLibrary/src/T4T_NDEF_emu.c"
        "NxpNci20/src/NxpNci20.c"

    INCLUDE_DIRS
        "inc"
        "NdefLibrary/inc"
        "NxpNci20/inc"

    REQUIRES
        driver main TML tool
)
