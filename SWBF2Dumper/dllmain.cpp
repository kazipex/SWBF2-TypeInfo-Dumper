#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <thread>
#include "SdkGenerator.h"
#include "json.hpp"
#include <fstream>
#include <optional>
#include "SdkGenerator.h"
#include "StaticOffsets.h" 

using namespace BackendBf4;
using namespace fb;
using namespace nlohmann;

std::string hexifyAddress(void* address) {
    char buf[64];
    sprintf_s(buf, "0x%p", address);
    return std::string(buf);
}

int lookupType(std::vector<fb::TypeInfo*> typeinfos, fb::TypeInfo* lt) {
    for (int i = 0; i < typeinfos.size(); i++)
        if (typeinfos[i] == lt) return i + 1;

    throw std::runtime_error("Could not find a type's internal id");
};

// EW EW EW EW
json SerializeEnumFieldInfoData(std::vector<fb::TypeInfo*>& typeinfos, EnumFieldInfo::EnumFieldInfoData::EnumFieldInfoDataField* t) {
    json j;

    j["name"] = std::string(t->m_Name);
    j["flags"] = t->m_Flags.m_FlagBits;
    j["offset"] = t->m_FieldOffset;

    return j;
}

json SerializeFieldInfoData(std::vector<fb::TypeInfo*>& typeinfos, FieldInfo::FieldInfoData* t) {
    json j;

    j["name"] = std::string(t->m_Name);
    j["flags"] = t->m_Flags.m_FlagBits;
    j["offset"] = t->m_FieldOffset;
    if(t->m_FieldTypePtr) j["type"] = lookupType(typeinfos, t->m_FieldTypePtr);

    return j;
}

json SerializeTypeInfo(std::vector<fb::TypeInfo*>& typeinfos, fb::TypeInfo* current) {
    json typeinfo;

    typeinfo["typeinfo"] = hexifyAddress(current);
    typeinfo["flags"] = current->m_Flags;
    typeinfo["runtimeId"] = current->m_RuntimeId;
    if (current->m_Next)
        typeinfo["next"] = lookupType(typeinfos, current->m_Next);

    json infoData;
    infoData["flags"] = current->m_InfoData->m_Flags.m_FlagBits;
    infoData["name"] = current->m_InfoData->m_Name;
    infoData["type"] = current->GetTypeCode();
    infoData["typename"] = current->GetTypeName();
    switch (current->GetTypeCode()) {
    case kTypeCode_Class: {
        auto ci = (ClassInfo*)current;
        typeinfo["classId"] = ci->m_ClassId;
        if (ci->m_DefaultInstance) typeinfo["defaultInstance"] = hexifyAddress(ci->m_DefaultInstance);
        //if (ci->m_FirstDerivedClass) typeinfo["firstDerivedClass"] = lookupType(typeinfos, ci->m_FirstDerivedClass);
        typeinfo["lastSubClassId"] = ci->m_LastSubClassId;
        if (ci->m_NextSiblingClass) typeinfo["nextSiblingClass"] = lookupType(typeinfos, ci->m_NextSiblingClass);
        if (ci->m_Super) typeinfo["superClass"] = lookupType(typeinfos, ci->m_Super);
        typeinfo["totalFieldCount"] = ci->m_TotalFieldCount;

        auto cid = ci->GetClassInfoData();
        infoData["alignment"] = cid->m_Alignment;
        infoData["fieldCount"] = cid->m_FieldCount;
        infoData["module"] = std::string(cid->m_Module->m_ModuleName);
        if (cid->m_pArrayTypeInfo) infoData["arrayTypeInfo"] = lookupType(typeinfos, cid->m_pArrayTypeInfo);
        infoData["totalSize"] = cid->m_TotalSize;
        json fields;
        for (int i = 0; i < cid->m_FieldCount; i++)
            fields.push_back(SerializeFieldInfoData(typeinfos, &cid->m_Fields[i]));
        infoData["fields"] = fields;
        break;
    }
    case kTypeCode_ValueType: {
        auto ci = (ValueTypeInfo*)current;
        auto cid = ci->GetValueInfoData();
        infoData["alignment"] = cid->m_Alignment;
        infoData["fieldCount"] = cid->m_FieldCount;
        infoData["module"] = std::string(cid->m_Module->m_ModuleName);
        if (cid->m_pArrayTypeInfo) infoData["arrayTypeInfo"] = lookupType(typeinfos, cid->m_pArrayTypeInfo);
        infoData["totalSize"] = cid->m_TotalSize;
        break;
    }
    case kTypeCode_FixedArray:
    case kTypeCode_Array: {
        auto i = (ArrayTypeInfo*)current;
        if(i->GetArrayTypeInfoData()->m_ElementType)
            infoData["arrayTypeInfo"] = lookupType(typeinfos, i->GetArrayTypeInfoData()->m_ElementType);
        infoData["module"] = std::string(i->GetArrayTypeInfoData()->m_Module->m_ModuleName);
        infoData["totalSize"] = i->GetArrayTypeInfoData()->m_TotalSize;
        break;
    }
    case kTypeCode_Enum: {
        auto ci = (EnumFieldInfo*)current;
        auto cid = ci->GetEnumInfoData();
        infoData["alignment"] = cid->m_Alignment;
        infoData["fieldCount"] = cid->m_FieldCount;
        infoData["module"] = std::string(cid->m_Module->m_ModuleName);
        if (cid->m_pArrayTypeInfo) infoData["arrayTypeInfo"] = lookupType(typeinfos, cid->m_pArrayTypeInfo);
        infoData["totalSize"] = cid->m_TotalSize;
        json fields;
        for (int i = 0; i < cid->m_FieldCount; i++)
            fields.push_back(SerializeEnumFieldInfoData(typeinfos, &cid->m_Fields[i]));
        infoData["fields"] = fields;
        break;
    }
    }

    typeinfo["info"] = infoData;
    return typeinfo;
}

// ---------------------------------------------------------------------
// NEW: quick live-instance test for WorldRenderSettings.
// Walks the already-built typeinfos list, finds the WorldRenderSettings
// ClassInfo entry, and — if it has a default instance — reads the
// TemporalAAResponsiveness float at the offset we confirmed from the
// dump (+0x384). This does NOT prove the pointer is "live" by itself;
// compare the printed value against your current InitFS setting across
// a couple of relaunches (with a distinctive InitFS value) to confirm.
// ---------------------------------------------------------------------
void TestWorldRenderSettingsDefaultInstance(std::vector<fb::TypeInfo*>& typeinfos) {
    fb::ClassInfo* wrsClass = nullptr;

    for (auto* t : typeinfos) {
        if (t->GetTypeCode() != kTypeCode_Class) continue;

        auto* info = t->GetTypeInfoData();
        if (!info || !info->m_Name) continue;
        if (IsBadReadPtr(info->m_Name, 1)) continue;

        if (std::string(info->m_Name) == "WorldRenderSettings") {
            wrsClass = (fb::ClassInfo*)t;
            break;
        }
    }

    if (!wrsClass) {
        printf("[WRS-TEST] Could not find WorldRenderSettings in the type list.\n");
        return;
    }

    printf("[WRS-TEST] Found WorldRenderSettings ClassInfo at 0x%p\n", wrsClass);

    if (!wrsClass->m_DefaultInstance) {
        printf("[WRS-TEST] m_DefaultInstance is NULL -- no default instance exists.\n");
        return;
    }

    void* instance = wrsClass->m_DefaultInstance;
    printf("[WRS-TEST] Default instance: 0x%p\n", instance);

    if (IsBadReadPtr(instance, 0x384 + sizeof(float))) {
        printf("[WRS-TEST] Default instance pointer is not readable at +0x384 -- bad pointer or wrong offset.\n");
        return;
    }

    float* taa = (float*)((uintptr_t)instance + 0x384);
    printf("[WRS-TEST] TemporalAAResponsiveness (+0x384) current value: %f\n", *taa);

    // Uncomment to test a live write once you've confirmed the read value
    // matches your InitFS setting across a relaunch:
    //
    // *taa = 0.9f;
    // printf("[WRS-TEST] Wrote 0.9 -- check in-game TAA behavior during camera motion.\n");
}

void Thread(HMODULE mod) {
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    freopen("CON", "w", stdout);

    printf("Dumping...\n");

    fb::TypeInfo* g_firstTypeInfo = *(fb::TypeInfo**)StaticOffsets::Get_OFFSET_FIRSTTYPEINFO();

    if (!g_firstTypeInfo || IsBadReadPtr(g_firstTypeInfo, sizeof(fb::TypeInfo))) {
        printf("Signature scan failed to resolve a valid TypeInfo pointer.\n");
        FreeConsole();
        return;
    }

    std::vector<fb::TypeInfo*> typeinfos;

    fb::TypeInfo* current = g_firstTypeInfo;
    do {
        typeinfos.push_back(current);
    } while (current = current->m_Next);

    // Run the WorldRenderSettings live-instance test before the (slower)
    // full JSON dump, so you get an answer immediately in the console.
    TestWorldRenderSettingsDefaultInstance(typeinfos);

    json results;
    int id = 0;
    for(auto current : typeinfos) {
        json d = SerializeTypeInfo(typeinfos, current);
        d["index"] = id;
        results.push_back(d);
        id++;
    }

    auto str = results.dump(4);
    std::ofstream f("dump.json");
    f << str;
    f.close();
    printf("Done\n");
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        std::thread(Thread, hModule).detach();

    return TRUE;
}
