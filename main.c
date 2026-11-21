#include <pspkernel.h>
#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

PSP_MODULE_INFO("MKModelSwapper", 0, 1, 1);

#define MAX_SWAPS 16
#define MAX_BONES 128
#define INI_PATH  "ms0:/PSP/PLUGINS/mkswap/settings.ini"
#define BONS_PATH "ms0:/PSP/PLUGINS/mkswap/bons.ini"

typedef struct {
    char modelChar;
    char fightChar;
    char newBody;
    char newHead;
    char newBodyAlt;
    int isCbm;
} SwapRule;

typedef struct {
    char name;
    unsigned int id;
    int parentIndex;
} BoneNode;

SwapRule rules[MAX_SWAPS];
int ruleCount = 0;
int bGetBons = 0;

void FormatName(char *str) {
    int i = 0, j = 0;
    while (str[i]) {
        if (!isspace((unsigned char)str[i])) {
            str[j++] = toupper((unsigned char)str[i]);
        }
        i++;
    }
    str[j] = '\0';
}

void ParseRuleContent(char *start, int isCbmMode) {
    if (ruleCount >= MAX_SWAPS) return;
    char *sep = strchr(start, '>');
    char *end = strchr(start, ')');

    if (sep != NULL && end != NULL && sep < end) {
        int len1 = sep - start;
        int len2 = end - (sep + 1);

        if (len1 > 0 && len1 < 31 && len2 > 0 && len2 < 31) {
            if (!isCbmMode) {
                // chang(charmudelname>fightcharname)
                strncpy(rules[ruleCount].modelChar, start, len1);
                rules[ruleCount].modelChar[len1] = '\0';
                strncpy(rules[ruleCount].fightChar, sep + 1, len2);
                rules[ruleCount].fightChar[len2] = '\0';
            } else {
                // cbm=(اسم الشخصية>اسم الموديل)
                strncpy(rules[ruleCount].fightChar, start, len1);
                rules[ruleCount].fightChar[len1] = '\0';
                strncpy(rules[ruleCount].modelChar, sep + 1, len2);
                rules[ruleCount].modelChar[len2] = '\0';
            }

            FormatName(rules[ruleCount].modelChar);
            FormatName(rules[ruleCount].fightChar);
            rules[ruleCount].isCbm = isCbmMode;

            sprintf(rules[ruleCount].newBody, "BODY_%s", rules[ruleCount].modelChar);
            sprintf(rules[ruleCount].newHead, "HEAD_%s", rules[ruleCount].modelChar);

            char lowerModel;
            strcpy(lowerModel, rules[ruleCount].modelChar);
            for (int k = 0; lowerModel[k]; k++) {
                lowerModel[k] = tolower((unsigned char)lowerModel[k]);
            }
            sprintf(rules[ruleCount].newBodyAlt, "body_%s_alt.sec", lowerModel);

            ruleCount++;
        }
    }
}

void ParseSettings(void) {
    SceUID fd = sceIoOpen(INI_PATH, PSP_O_RDONLY, 0777);
    if (fd < 0) return;

    char buffer;
    int bytesRead = sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);

    if (bytesRead <= 0) return;
    buffer[bytesRead] = '\0';

    char *line = strtok(buffer, "\r\n");
    while (line != NULL) {
        // إزالة المسافات في بداية السطر
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
    if (ruleCount == 0) return;

    unsigned int start_addr = 0x08800000;
    unsigned int end_addr   = 0x09800000;

    for (int r = 0; r < ruleCount; r++) {
        char targetBody, targetHead, targetAlt;
        sprintf(targetBody, "BODY_%s", rules[r].fightChar);
        sprintf(targetHead, "HEAD_%s", rules[r].fightChar);

        char lowerFight;
        strcpy(lowerFight, rules[r].fightChar);
        for (int k = 0; lowerFight[k]; k++) {
            lowerFight[k] = tolower((unsigned char)lowerFight[k]);
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
                *(unsigned int*)ptrAddr = (unsigned int)rules[r].newBody;
            } else if (origHeadAddr && val == origHeadAddr) {
                *(unsigned int*)ptrAddr = (unsigned int)rules[r].newHead;
            } else if (origAltAddr && val == origAltAddr) {
                *(unsigned int*)ptrAddr = (unsigned int)rules[r].newBodyAlt;
            }
        }
    }

    sceKernelDcacheWritebackInvalidateAll();
}

// دالة مساعدة لكتابة شجرة العظام بمسافات بادئة حسب الارتباط الهرمي
void WriteBoneHierarchy(SceUID fd, BoneNode *bones, int count, int parentIdx, int depth) {
    char lineOut;
    char indent;
    int spaces = depth * 4;
    if (spaces > 60) spaces = 60;
    memset(indent, ' ', spaces);
    indent[spaces] = '\0';

    for (int i = 0; i < count; i++) {
        if (bones[i].parentIndex == parentIdx) {
            int len = sprintf(lineOut, "%s[%s]=0x%02X\r\n", indent, bones[i].name, bones[i].id);
            sceIoWrite(fd, lineOut, len);
            WriteBoneHierarchy(fd, bones, count, i, depth + 1);
        }
    }
}

// استخراج هيكل العظام من ذاكرة اللعبة وكتابته في ملف bons.ini
void DumpBonesToFile(void) {
    if (!bGetBons) return;

    SceUID fd = sceIoOpen(BONS_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;

    unsigned int start_addr = 0x08800000;
    unsigned int end_addr   = 0x09C00000;
    char header;

    // البحث عن رؤوس بيانات الشخصيات المحمّلة في الذاكرة (تبدأ بـ BODY_)
    for (unsigned int addr = start_addr; addr < end_addr; addr += 4) {
        if (memcmp((void*)addr, "BODY_", 5) == 0) {
            char *charName = (char*)(addr + 5);
            if (strlen(charName) > 2 && strlen(charName) < 20 && isupper((unsigned char)charName)) {
                int len = sprintf(header, "[%s]\r\n", charName);
                sceIoWrite(fd, header, len);

                BoneNode bones[MAX_BONES];
                int boneCount = 0;

                // مسح النطاق التالي في الذاكرة للبحث عن أسماء العظام المرتبطة (BONE_ أو BIP)
                unsigned int scanLimit = addr + 0x40000;
                if (scanLimit > end_addr) scanLimit = end_addr;

                for (unsigned int bAddr = addr + 16; bAddr < scanLimit && boneCount < MAX_BONES; bAddr += 4) {
                    if (memcmp((void*)bAddr, "BONE_", 5) == 0 || memcmp((void*)bAddr, "BIP", 3) == 0) {
                        char *bName = (char*)bAddr;
                        int nLen = strlen(bName);
                        if (nLen > 3 && nLen < 31) {
                            strcpy(bones[boneCount].name, bName);
                            // قراءة الرقم الكودي ومؤشر الأب من البنية المجاورة في الذاكرة
                            unsigned int rawId = *(unsigned int*)(bAddr - 8);
                            int rawParent = *(int*)(bAddr - 4);

                            bones[boneCount].id = (rawId < 256) ? rawId : (unsigned int)boneCount;
                            bones[boneCount].parentIndex = (rawParent >= -1 && rawParent < boneCount) ? rawParent : (boneCount - 1);
                            boneCount++;
                            bAddr += (nLen & ~3);
                        }
                    }
                }

                if (boneCount > 0) {
                    WriteBoneHierarchy(fd, bones, boneCount, -1, 1);
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
    ParseSettings();

    if (ruleCount > 0) {
        // تطبيق تبديل المجسمات والعظام بعد تحميل جدول الشخصيات
        sceKernelDelayThread(4000000);
        ApplyPointerSwaps();
    }

    if (bGetBons) {
        // انتظار إضافي حتى يدخل اللاعب في شاشة القتال ويتم تحميل هيكل العظام في الذاكرة
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
    return 0;
}
