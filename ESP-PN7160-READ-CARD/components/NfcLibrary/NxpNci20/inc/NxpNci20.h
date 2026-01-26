/*
*         Copyright (c), NXP Semiconductors Caen / France
*
*                     (C)NXP Semiconductors
*       All rights are reserved. Reproduction in whole or in part is
*      prohibited without the written consent of the copyright owner.
*  NXP reserves the right to make changes without notice at any time.
* NXP makes no warranty, expressed, implied or statutory, including but
* not limited to any implied warranty of merchantability or fitness for any
*particular purpose, or that the use will not infringe any third party patent,
* copyright or trademark. NXP must not be liable for any loss or damage
*                          arising from its use.
*/

#include <Nfc.h>

#ifndef REMOVE_CARDEMU_SUPPORT
#include <T4T_NDEF_emu.h>
#endif //#ifndef REMOVE_CARDEMU_SUPPORT

#ifndef REMOVE_P2P_SUPPORT
#include <P2P_NDEF.h>
#endif //#ifndef REMOVE_P2P_SUPPORT

#ifndef REMOVE_RW_SUPPORT
#include <RW_NDEF.h>
#include <RW_NDEF_T3T.h>
#endif //#ifndef REMOVE_RW_SUPPORT

#define NXPNCI_SUCCESS      NFC_SUCCESS
#define NXPNCI_ERROR        NFC_ERROR
#define NCI_DEBUG

#ifdef NCI_DEBUG
#define NCI_PRINT(...)        {PRINTF(__VA_ARGS__);}
unsigned short debug_loop;
#define NCI_PRINT_BUF(x,y,z)  {PRINT_BUF(x,y,z);}
#else
#define NCI_PRINT(...)
#define NCI_PRINT_BUF(x,y,z)
#endif

