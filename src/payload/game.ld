/* Vagrant Story (USA) symbol addresses, from the Rood Reverse decomp. */

/* SLUS_010.40: Psy-Q libraries */
FlushCache = 0x80026874;
open = 0x80026994;
read = 0x800269A4;
write = 0x800269B4;
close = 0x800269C4;
erase = 0x800269F4;
VSync = 0x8001F6C4;
FntLoad = 0x80027894;
FntOpen = 0x80027934;
FntFlush = 0x80027BEC;
FntPrint = 0x80027F08;
DrawSync = 0x80028650;
ClearImage = 0x800287D4;
LoadImage = 0x800288FC;
StoreImage = 0x8002895C;
DrawPrim = 0x80028BE8;
PutDrawEnv = 0x80028CB4;
PutDispEnv = 0x80028E80;
SetDefDrawEnv = 0x8002B374;
SetDefDispEnv = 0x8002B434;

/* SLUS_010.40: game */
GAME_ENTRY = 0x8001F544; /* original exe pc0 */
_loadTitlePrg = 0x80041F34;
vs_main_gametimeUpdate = 0x8004261C;
vs_main_updatePadState = 0x800430F4;
vs_main_processPadState = 0x80043940;
vs_main_freeHeap = 0x80043D50;
vs_main_allocHeap = 0x80043EC4;
vs_main_allocateCdQueueSlot = 0x80044B10;
vs_main_freeCdQueueSlot = 0x80044B80;
vs_main_cdEnqueuePriority = 0x80044BF4;
vs_main_memcpy = 0x800490B0;
vs_main_bzero = 0x80049110;
vs_main_startState = 0x80050470;
vs_main_actions = 0x8004B9DC;
vs_main_artsPointsRequirements = 0x8004FDE4;
vs_main_battleAbilitiesPointsRequirements = 0x8004FE5C;
vs_main_buttonRepeat = 0x8005DFDC;
vs_main_buttonsPressed = 0x8005E1D0;
vs_main_buttonsState = 0x8005E238;
vs_main_inventory = 0x80060168;
vs_main_stateFlags = 0x80061598;

/* BATTLE.PRG (overlay slot 0) */
func_80076F24 = 0x80076F24;
_loadZndEnemies = 0x80088D40;
_loadMpdRoomSection = 0x8008A908;
vs_battle_renderTextRaw = 0x800C6828;
_zoneContext = 0x800F1880;
D_800F1AB0 = 0x800F1AB0;

/* MAINMENU.PRG (overlay slot 1) */
vs_mainMenu_exec = 0x800FAAC8;

/* MENU0.PRG (magic menu, loaded at 0x80102800) */
menu0_setMpCost = 0x801029B8; /* _setMPCostDirect */
menu0_teleportMp = 0x80106A0C; /* _teleportMenu's currentMp */
