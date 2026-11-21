#include <pspkernel.h>
#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <pspsysmem.h>
#include <string.h>
#include <stdio.h>

PSP_MODULE_INFO("MKModelSwapper", 0, 1, 1);

#define MAX_SWAPS 16
#define MAX_BONES 128
#define INI_PATH  "ms0:/PSP/PLUGINS/mkswap/settings.ini"
#define BONS_PATH "ms0:/PSP/PLUGINS/mkswap/bons.ini"

static SceUID memId = -1;
static unsigned char *memPool = NULL;
static unsigned int memOffset = 0;
static unsigned int memTotalSize = 32768;

int MyIsSpace(unsigned char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f');
}

int MyIsUpper(unsigned char c) {
    return (c >= 'A' && c <= 'Z');
}

unsigned char MyToUpper(unsigned char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

unsigned char MyToLower(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

void InitMemory(void) {
    memId = sceKernelAllocPartitionMemory(2, "MKSwapPool", PSP_SMEM_Low, memTotalSize, NULL);
    if (memId >= 0) {
        memPool = (unsigned char*)sceKernelGetBlockHeadAddr(memId);
        memset(memPool, 0, memTotalSize);
    }
}

void* AllocMem(unsigned int size) {
    if (!memPool) return NULL;
    size = (size + 3) & ~3;
    if (memOffset + size >= memTotalSize) return NULL;
    void *ptr = (void*)(memPool + memOffset);
    memOffset += size;
    return ptr;
}

typedef struct {
    char *modelChar;
    char *fightChar;
    char *newBody;
    char *newHead;
    char *newBodyAlt;
    int isCbm;
} SwapRule;

typedef struct {
    char *name;
    unsigned int id;
    int parentIndex;
} BoneNode;

static SwapRule *rules = NULL;
static int ruleCount = 0;
static int bGetBons = 0;

void FormatName(char *str) {
    int i = 0, j = 0;
    while (*(str + i)) {
        unsigned char c = (unsigned char)(*(str + i));
        if (!MyIsSpace(c)) {
            *(str + j) = (char)MyToUpper(c);
            j++;
        }
        i++;
    }
    *(str + j) = '\0';
}

void ParseRuleContent(char *start, int isCbmMode) {
    if (ruleCount >= MAX_SWAPS || !rules) return;
    char *sep = strchr(start, '>');
    char *end = strchr(start, ')');

    if (sep != NULL && end != NULL && sep < end) {
        int len1 = sep - start;
        int len2 = end - (sep + 1);

        if (len1 > 0 && len1 < 31 && len2 > 0 && len2 < 31) {
            SwapRule *r = rules + ruleCount;
            r->modelChar  = (char*)AllocMem(32);
            r->fightChar  = (char*)AllocMem(32);
            r->newBody    = (char*)AllocMem(48);
            r->newHead    = (char*)AllocMem(48);
            r->newBodyAlt = (char*)AllocMem(64);

            if (!isCbmMode) {
                strncpy(r->modelChar, start, len1);
                *(r->modelChar + len1) = '\0';
                strncpy(r->fightChar, sep + 1, len2);
                *(r->fightChar + len2) = '\0';
            } else {
                strncpy(r->fightChar, start, len1);
                *(r->fightChar + len1) = '\0';
                strncpy(r->modelChar, sep + 1, len2);
                *(r->modelChar + len2) = '\0';
            }

            FormatName(r->modelChar);
            FormatName(r->fightChar);
            r->isCbm = isCbmMode;

            sprintf(r->newBody, "BODY_%s", r->modelChar);
            sprintf(r->newHead, "HEAD_%s", r->modelChar);

            char *lowerModel = (char*)AllocMem(32);
            strcpy(lowerModel, r->modelChar);
            for (int k = 0; *(lowerModel + k); k++) {
                *(lowerModel + k) = (char)MyToLower((unsigned char)(*(lowerModel + k)));
            }
            sprintf(r->newBodyAlt, "body_%s_alt.sec", lowerModel);

            ruleCount++;
        }
    }
}

void ParseSettings(void) {
    rules = (SwapRule*)AllocMem(sizeof(SwapRule) * MAX_SWAPS);
    SceUID fd = sceIoOpen(INI_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return;

    char *buffer = (char*)AllocMem(4096);
    if (!buffer) {
        sceIoClose(fd);
        return;
    }

    int bytesRead = sceIoRead(fd, buffer, 4095);
    sceIoClose(fd);

    if (bytesRead <= 0) return;
    *(buffer + bytesRead) = '\0';

    char *line = strtok(buffer, "\r\n");
    while (line != NULL) {
        while (*line == ' ' || *line == '\t') line++;

        if (*line != ';' && *line != '#' && *line != '\0') {
            if (strstr(line, "getbons=true") != NULL || strstr(line, "gitbon=true") != NULL) {
                bGetBons = 1;
            } else if (strncmp(line, "chang(", 6) == 0) {
                ParseRuleContent(line + 6, 0);
            } else if (strncmp(line, "cbm=(", 5) == 0) {
                ParseRuleContent(line + 5, 1);
            }
        }
        line = strtok(NULL, "\r\n");
    }
}

void ApplyPointerSwaps(void) {
    if (ruleCount == 0 || !rules) return;

    unsigned int start_addr = 0x08800000;
    unsigned int end_addr   = 0x09800000;

    char *targetBody = (char*)AllocMem(48);
    char *targetHead = (char*)AllocMem(48);
    char *targetAlt  = (char*)AllocMem(64);
    char *lowerFight = (char*)AllocMem(32);

    for (int i = 0; i < ruleCount; i++) {
        SwapRule *r = rules + i;
        sprintf(targetBody, "BODY_%s", r->fightChar);
        sprintf(targetHead, "HEAD_%s", r->fightChar);

        strcpy(lowerFight, r->fightChar);
        for (int k = 0; *(lowerFight + k); k++) {
            *(lowerFight + k) = (char)MyToLower((unsigned char)(*(lowerFight + k)));
        }
        sprintf(targetAlt, "body_%s_alt.sec", lowerFight);

        unsigned int origBodyAddr = 0, origHeadAddr = 0, origAltAddr = 0;

        for (unsigned int addr = start_addr; addr < end_addr; addr += 4) {
            if (!origBodyAddr && strcmp((char*)addr, targetBody) == 0) origBodyAddr = addr;
            else if (!origHeadAddr && strcmp((char*)addr, targetHead) == 0) origHeadAddr = addr;
            else if (!origAltAddr && strcmp((char*)addr, targetAlt) == 0) origAltAddr = addr;

            if (origBodyAddr && origHeadAddr && origAltAddr) break;
        }

        for (unsigned int ptrAddr = start_addr; ptrAddr < end_addr; ptrAddr += 4) {
            unsigned int val = *(unsigned int*)ptrAddr;
            if (origBodyAddr && val == origBodyAddr) {
                *(unsigned int*)ptrAddr = (unsigned int)r->newBody;
            } else if (origHeadAddr && val == origHeadAddr) {
                *(unsigned int*)ptrAddr = (unsigned int)r->newHead;
            } else if (origAltAddr && val == origAltAddr) {
                *(unsigned int*)ptrAddr = (unsigned int)r->newBodyAlt;
            }
        }
    }

    sceKernelDcacheWritebackInvalidateAll();
}

void WriteBoneHierarchy(SceUID fd, BoneNode *bones, int count, int parentIdx, int depth, char *lineOut, char *indent) {
    int spaces = depth * 4;
    if (spaces > 60) spaces = 60;
    memset(indent, ' ', spaces);
    *(indent + spaces) = '\0';

    for (int i = 0; i < count; i++) {
        BoneNode *b = bones + i;
        if (b->parentIndex == parentIdx) {
            int len = sprintf(lineOut, "%s[%s]=0x%02X\r\n", indent, b->name, b->id);
            sceIoWrite(fd, lineOut, len);
            WriteBoneHierarchy(fd, bones, count, i, depth + 1, lineOut, indent);
        }
    }
}

void DumpBonesToFile(void) {
    if (!bGetBons) return;

    SceUID fd = sceIoOpen(BONS_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;

    unsigned int start_addr = 0x08800000;
    unsigned int end_addr   = 0x09C00000;

    char *header  = (char*)AllocMem(64);
    char *lineOut = (char*)AllocMem(128);
    char *indent  = (char*)AllocMem(64);
    BoneNode *bones = (BoneNode*)AllocMem(sizeof(BoneNode) * MAX_BONES);

    for (int k = 0; k < MAX_BONES; k++) {
        (bones + k)->name = (char*)AllocMem(36);
    }

    for (unsigned int addr = start_addr; addr < end_addr; addr += 4) {
        if (memcmp((void*)addr, "BODY_", 5) == 0) {
            char *charName = (char*)(addr + 5);
            if (strlen(charName) > 2 && strlen(charName) < 20 && MyIsUpper((unsigned char)(*charName))) {
                int len = sprintf(header, "[%s]\r\n", charName);
                sceIoWrite(fd, header, len);

                int boneCount = 0;
                unsigned int scanLimit = addr + 0x40000;
                if (scanLimit > end_addr) scanLimit = end_addr;

                for (unsigned int bAddr = addr + 16; bAddr < scanLimit && boneCount < MAX_BONES; bAddr += 4) {
                    if (memcmp((void*)bAddr, "BONE_", 5) == 0 || memcmp((void*)bAddr, "BIP", 3) == 0) {
                        char *bName = (char*)bAddr;
                        int nLen = strlen(bName);
                        if (nLen > 3 && nLen < 32) {
                            BoneNode *curr = bones + boneCount;
                            strcpy(curr->name, bName);

                            unsigned int rawId = *(unsigned int*)(bAddr - 8);
                            int rawParent = *(int*)(bAddr - 4);

                            curr->id = (rawId < 256) ? rawId : (unsigned int)boneCount;
                            curr->parentIndex = (rawParent >= -1 && rawParent < boneCount) ? rawParent : (boneCount - 1);
                            boneCount++;
                            bAddr += (nLen & ~3);
                        }
                    }
                }

                if (boneCount > 0) {
                    WriteBoneHierarchy(fd, bones, boneCount, -1, 1, lineOut, indent);
                } else {
                    const char *noData = "    ; No active skeleton loaded in memory yet\r\n";
                    sceIoWrite(fd, noData, strlen(noData));
                }
                sceIoWrite(fd, "\r\n", 2);
            }
        }
    }

    sceIoClose(fd);
}

int MainThread(SceSize args, void *argp) {
    InitMemory();
    ParseSettings();

    if (ruleCount > 0) {
        sceKernelDelayThread(4000000);
        ApplyPointerSwaps();
    }

    if (bGetBons) {
        sceKernelDelayThread(12000000);
        DumpBonesToFile();
    }

    return sceKernelExitDeleteThread(0);
}
int module_start(SceSize args, void *argp) {
SceUID thid = sceKernelCreateThread("MKSwapThread", MainThread, 0x18, 0x10000, 0, NULL);
if (thid >= 0) {
sceKernelStartThread(thid, args, argp);
}
return 0;
}
int module_stop(SceSize args, void *argp) {
if (memId >= 0) {
sceKernelFreePartitionMemory(memId);
}
return 0;
}
