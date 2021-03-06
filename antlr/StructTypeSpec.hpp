/*
 * This work was authored by Two Six Labs, LLC and is sponsored by a subcontract
 * agreement with Galois, Inc.  This material is based upon work supported by
 * the Defense Advanced Research Projects Agency (DARPA) under Contract No.
 * HR0011-19-C-0103.
 *
 * The Government has unlimited rights to use, modify, reproduce, release,
 * perform, display, or disclose computer software or computer software
 * documentation marked with this legend. Any reproduction of technical data,
 * computer software, or portions thereof marked with this legend must also
 * reproduce this marking.
 *
 * Copyright 2020 Two Six Labs, LLC.  All rights reserved.
 */

#pragma once

#include "CDRTypes.hpp"
#include "CDRGenerator.hpp"

class StructMember {
public:
    TypeSpec* typeSpec;
    std::vector<Declarator*> declarators;
    StructMember(TypeSpec* typeSpec) : typeSpec(typeSpec), declarators() { }
    void addDeclarator(Declarator* declarator);
    ~StructMember();
};

typedef std::function<void(StructMember* member, Declarator* declarator)> StructFunction;

// Implementation of the struct type
class StructTypeSpec : public TypeSpec {
private:
    void cDeclareFunctionApply(bool scalar, bool array, StructFunction apply);
    void cCppFunctionBody(std::ostream &ostream, CDRFunc functionType, TargetLanguage languageType);
    void cCppTypeDecl(std::ostream &ostream, TargetLanguage languageType);
    void cCppTypeDeclWire(std::ostream &ostream, TargetLanguage languageType);
    void cppDeclareSerializationFunction(std::ostream &ostream);
    void cppDeclareDeserializationFunction(std::ostream &ostream);
    void cppDeclareInternalSerializationFunction(std::ostream &ostream);
    void cppDeclareInternalDeserializationFunction(std::ostream &ostream);
public:
    std::string namespacePrefix;
    std::string identifier;
    bool packed;
    std::vector<StructMember*> members;
    StructTypeSpec(std::string namespacePrefix, std::string identifier, bool packed) :
        namespacePrefix(namespacePrefix), identifier(identifier),
        packed(packed), members() { }
    virtual CDRTypeOf typeOf() override { return CDRTypeOf::STRUCT_T; }
    virtual void cTypeDecl(std::ostream &ostream) override;
    virtual void cTypeDeclWire(std::ostream &ostream) override;
    virtual std::string cTypeName() override { return "struct " + identifier; }
    virtual std::string cppTypeName() override { return "struct " + identifier; }
    virtual std::string identifierName() override { return identifier; }
    virtual std::string cppNamespacePrefix() override { return namespacePrefix; }
    virtual CDRBits cTypeBits() override { return CDRBits::UNDEFINED; }
    virtual void cDeclareFunctions(std::ostream &ostream, CDRFunc functionType) override;
    virtual void cDeclareAnnotationValidate(std::ostream &ostream) override;
    virtual void cDeclareAnnotationTransform(std::ostream &ostream) override;
    virtual void cDeclareAsserts(std::ostream &ostream) override;
    virtual void cppTypeDecl(std::ostream &ostream) override;
    virtual void cppTypeDeclWire(std::ostream &ostream) override;
    virtual void cppDeclareAsserts(std::ostream &ostream) override { cDeclareAsserts(ostream); }
    virtual void cppDeclareFunctions(std::ostream &ostream) override;
    virtual bool container() override { return true; }
    void addMember(StructMember* member);
    virtual ~StructTypeSpec();
};
