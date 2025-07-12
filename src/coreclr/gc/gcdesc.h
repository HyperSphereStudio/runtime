// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
//
// GC Object Pointer Location Series Stuff
//

#ifndef _GCDESC_H_
#define _GCDESC_H_

#ifdef HOST_64BIT
typedef uint32_t HALF_SIZE_T;
#else   // HOST_64BIT
typedef uint16_t HALF_SIZE_T;
#endif

typedef DPTR(class MethodTable) PTR_MethodTable;
typedef DPTR(class CGCDesc) PTR_CGCDesc;
typedef size_t CGCUnionDescOffset;
typedef uint32_t CGCUnionOffsetTableOffset;
typedef uint32_t CGCEntryTableIndex;
typedef uint32_t UnionTypeType;

#define CGCUnionDescOffsetNull 0

class alignas(size_t) CGCDescEntry{
public:

    void SetPointerSpan(uint32_t nptrs, uint32_t offset) {
        _ASSERTE (nptrs <= MaxPointerSpanLen);
        this->nptrs_or_unionofftableoffset = nptrs;
        this->offset = offset + sizeof(MethodTable*);
    }

    void SetUnion(uint32_t unionTypeOffset, uint32_t unionOffsetTableOffset) {
        _ASSERTE (unionOffsetTableOffset <= MaxUnionTableOffset);
        this->offset = unionTypeOffset + sizeof(MethodTable*);
        this->nptrs_or_unionofftableoffset = unionOffsetTableOffset;
        this->isuniontype = true;
    }

    uint32_t GetPointerSpanLength() {
        return nptrs_or_unionofftableoffset;
    }

    CGCUnionOffsetTableOffset GetUnionOffsetTableOffset() {
        return nptrs_or_unionofftableoffset;
    }

    uint32_t GetOffset() {
        return offset;
    }

    bool IsUnionType() {
        return isuniontype;
    }

    static const uint32_t MaxPointerSpanLen = ((uint32_t) -1) >> 1;
    static const uint32_t MaxUnionTableOffset = ((uint32_t) -1) >> 1;

private:
    uint32_t offset;
    uint32_t nptrs_or_unionofftableoffset : 31;
    uint32_t isuniontype : 1;
};

class alignas(size_t) CGCUnionOffsetTable {
    public:

    CGCUnionDescOffset GetUnionDescOffset(UnionTypeType unionType) {
        if (unionType == 0 || unionType > num_managed_types)
            return CGCUnionDescOffsetNull;
        return *(((CGCUnionDescOffset*) this) - unionType);
    }

    void Init(size_t num_managed_types) {
        this->num_managed_types = (uint16_t) num_managed_types;
    }

    size_t GetNumOffsets() {
        return num_managed_types;
    }

    private:
    uint32_t num_managed_types;
    uint32_t reserved;
};

class alignas(size_t) CGCDesc{
    
public:

#ifndef DACCESS_COMPILE
    void Init (size_t numEntries, size_t descSize, bool isValueArray){
        this->m_DescSize = (uint32_t) descSize;
        this->m_EntryCount = (uint32_t) numEntries;
        this->m_IsValueArray = isValueArray;
        this->m_MaxPointersPerComp = (uint32_t) this->GetMaxNumPointersPerComp();
    }
#endif

    static PTR_CGCDesc GetCGCDescFromMT (MethodTable * pMT)
    {
        // If it doesn't contain pointers, there isn't a GCDesc
        PTR_MethodTable mt(pMT);
#ifndef SOS_INCLUDE
        _ASSERTE(mt->ContainsGCPointers());
#endif
        return PTR_CGCDesc(mt) - 1;
    }

    size_t GetNumEntries (){
        return m_EntryCount;
    }

    bool IsValueArray (){
        return m_IsValueArray;
    }

    // Size of the entire slot map.
    size_t GetSize(){
        return m_DescSize;
    }

    uint8_t *GetStartOfGCData(){
        return ((uint8_t *)this) - (m_DescSize - sizeof(CGCDesc));
    }

    CGCDescEntry* GetEntry(CGCEntryTableIndex i) {
        _ASSERTE(i < (uint32_t) m_EntryCount);
        return (CGCDescEntry*) this - (i + 1);
    }

    CGCUnionOffsetTable* GetUnionOffsetTable(CGCUnionOffsetTableOffset off) {
        return (CGCUnionOffsetTable*) ((uint8_t*) this - off);
    }

    CGCDesc* GetUnionDesc(CGCUnionDescOffset off) {
        //Is inside the union desc table section?
        auto unionDescEndOffset = m_DescSize - sizeof(CGCDesc);
        _ASSERTE(off <= unionDescEndOffset);
        return (CGCDesc*) ((uint8_t*) this - off); 
    }

    static size_t GetMaxNumPointers(MethodTable* pMT, size_t ObjectSize, size_t NumComponents) {
          size_t NumOfPointers = 0;
          if (pMT->ContainsGCPointers()){
              NumOfPointers += GetCGCDescFromMT(pMT)->m_MaxPointersPerComp * NumComponents;
          }

#ifndef FEATURE_NATIVEAOT
     if (pMT->Collectible()){
            NumOfPointers += 1;
     }
#endif

         return NumOfPointers;
    }

    size_t GetMaxNumPointersPerComp(){
        size_t NumOfPointers = 0;
        for (int32_t i1 = (int32_t) GetNumEntries() - 1; i1 > -1; i1--) {
             CGCDescEntry* e1 = GetEntry(i1);
            if (e1->IsUnionType()) {
                size_t MaxNumPtsInUnion = 0;
                CGCUnionOffsetTable* ot = GetUnionOffsetTable(e1->GetUnionOffsetTableOffset());
                for (size_t i2 = ot->GetNumOffsets() - 1; i2 > 0; i2--) {
                    auto uDOff = ot->GetUnionDescOffset((UnionTypeType) i2);
                    MaxNumPtsInUnion = max(MaxNumPtsInUnion, GetUnionDesc(uDOff)->GetMaxNumPointersPerComp());
                }
                NumOfPointers += MaxNumPtsInUnion;
            }else{
                NumOfPointers += e1->GetPointerSpanLength();
            }
        }
        return NumOfPointers;
    }

private:
    uint32_t m_DescSize;
    uint32_t m_MaxPointersPerComp;
    uint32_t m_EntryCount: 31;
    uint32_t m_IsValueArray : 1;

};

#endif // _GCDESC_H_
