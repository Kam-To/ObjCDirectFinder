//
//  ObjCDirectFinder.cpp
//  DirectableFinder
//
//  Created by Kam on 2021/10/23.
//

#include "clang/Frontend/FrontendPluginRegistry.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MD5.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include <map>
#include <type_traits>


using namespace clang::tooling;
using namespace clang;
using namespace std;
using namespace llvm::json;

class Tool {
public:
    static string generateReadableName(const ObjCInterfaceDecl *interfaceDecl, ObjCMethodDecl *meth) {
        if (!interfaceDecl || !meth) return "";
        auto sign = meth->isInstanceMethod() ? "-" : "+";
        auto clzName = interfaceDecl->getNameAsString();
        auto methName = meth->getNameAsString();
        return sign + string("[") + clzName + string(" ") + methName + string("]");
    }

    static string generateReadableName(const ObjCCategoryDecl *categoryDecl, ObjCMethodDecl *meth) {
        if (!categoryDecl || !meth) return "";
        auto sign = meth->isInstanceMethod() ? "-" : "+";
        auto clzName = categoryDecl->getClassInterface()->getNameAsString();
        auto categoryName = categoryDecl->getNameAsString();
        auto methName = meth->getNameAsString();
        return sign + string("[") + clzName + string("(") + categoryName + string(")") + string(" ") + methName + string("]");
    }
};
class DirectableEntry {
    string fullName;
    string selectorName;
    string firstDeclLocation;
public:
    bool isPropertyAccessor;
    string getFullName() { return fullName; }
    string getSelectorName() { return selectorName; }
    string getFirstDeclLocation() { return firstDeclLocation; }
    
    DirectableEntry(ObjCInterfaceDecl *interfaceDecl, ObjCMethodDecl *meth, string firstDeclLoc)
    : DirectableEntry(Tool::generateReadableName(interfaceDecl, meth), meth->getSelector().getAsString(), firstDeclLoc, meth->isPropertyAccessor()) {}
    DirectableEntry(ObjCCategoryDecl *categoryDecl, ObjCMethodDecl *meth, string firstDeclLoc)
    : DirectableEntry(Tool::generateReadableName(categoryDecl, meth), meth->getSelector().getAsString(), firstDeclLoc, meth->isPropertyAccessor()) {}
    DirectableEntry(string f, string sel, string firstDecl, bool isPropertyAccessor)
    :fullName(f), selectorName(sel), firstDeclLocation(firstDecl), isPropertyAccessor(isPropertyAccessor) {}
};
class DirectableRecorder {
    
    // selector : [meth_name]
    map<string, list<DirectableEntry *> *> storage;
    
    // undirectable record
    map<string, Selector> undirectableSeletorRefList;
    set<string> undirectableReadableList;
    
    void insert(DirectableEntry *entry) {
        string key = entry->getSelectorName();
        if (undirectableSeletorRefList.find(key) != undirectableSeletorRefList.end()) {
            return;
        }
        
        if (undirectableReadableList.find(entry->getFullName()) != undirectableReadableList.end()) {
            return;
        }

        auto listTarget = storage.find(key);
        list<DirectableEntry *> *mList;
        if (listTarget != storage.end()) {
            mList = listTarget->second;
        } else {
            mList = new list<DirectableEntry *>();
            storage.insert(pair<string, list<DirectableEntry *> *>(key, mList));
        }
        
        const auto name = entry->getFullName();
        bool exist = false;
        for (auto j = mList->begin(), k = mList->end(); j != k; j++) {
            auto methPtr = *j;
            if (methPtr->getFullName() == name) {
                exist = true;
                break;
            }
        }
        
        if (!exist) {
            mList->push_back(entry);
        } else {
            delete entry;
        }
    }

public:
    string name;
    CompilerInstance &compilerInstance;
    DirectableRecorder(CompilerInstance &CI) : compilerInstance(CI) {
        storage = map<string, list<DirectableEntry *> *>();
        undirectableSeletorRefList = map<string, Selector>();
        undirectableReadableList = set<string>();
    }
    
    ~DirectableRecorder() {
        dump();
    }
    
    CompilerInstance &getCompilerInstance() {
        return compilerInstance;
    }
    
    // for class interface
    void insertDirectablePropertyGetterMethod(ObjCInterfaceDecl *interfaceDecl, const ObjCPropertyDecl *pro) {
        string firstDeclLoc = pro->getLocation().printToString(compilerInstance.getSourceManager());
        auto entry = new DirectableEntry(interfaceDecl, pro->getGetterMethodDecl(), firstDeclLoc);
        insert(entry);
    }
    
    void insertDirectableMethod(ObjCInterfaceDecl *interfaceDecl, ObjCMethodDecl *meth) {
        string firstDeclLoc = meth->getLocation().printToString(compilerInstance.getSourceManager());
        auto entry = new DirectableEntry(interfaceDecl, meth, firstDeclLoc);
        insert(entry);
    }
    
    // for category
    void insertDirectablePropertyGetterMethod(ObjCCategoryDecl *categoryDecl, const ObjCPropertyDecl *pro) {
        string firstDeclLoc = pro->getLocation().printToString(compilerInstance.getSourceManager());
        auto entry = new DirectableEntry(categoryDecl, pro->getGetterMethodDecl(), firstDeclLoc);
        insert(entry);
    }
    
    void insertDirectableMethod(ObjCCategoryDecl *categoryDecl, ObjCMethodDecl *meth) {
        string firstDeclLoc = meth->getLocation().printToString(compilerInstance.getSourceManager());
        auto entry = new DirectableEntry(categoryDecl, meth, firstDeclLoc);
        insert(entry);
    }

    void insertUndirectableMethodName(string name) {
        if (name.empty()) return;
        undirectableReadableList.insert(name);
        earseMarkedDirectMethodName(name);
    }

    void insertToUndirectableSel(Selector sel) {
        string selName = sel.getAsString();
        earseMarkedDirectMethodNBySel(selName);
        undirectableSeletorRefList.insert(pair<string, Selector>(selName, sel));
    }
    
    void earseMarkedDirectMethodNBySel(string selName) {
        auto listTarget = storage.find(selName);
        if (listTarget != storage.end()) {
            list<DirectableEntry *> *mList = listTarget->second;
            for (auto i = mList->begin(), e = mList->end(); i != e ; i++) {
                delete *i;
            }
            delete mList;
            storage.erase(selName);
        }
    }
    
    void earseMarkedDirectMethodName(string name) {
        size_t pos = name.find(" ") + 1;
        size_t len = name.length() - pos;
        auto selectorName = name.substr(pos, len - 1);
        auto listTarget = storage.find(selectorName);
        if (listTarget != storage.end()) {
            list<DirectableEntry *> *mList = listTarget->second;
            for (auto j = mList->begin(), k = mList->end(); j != k ; j++) {
                auto methPtr = *j;
                if (methPtr->getFullName() == name) {
                    delete methPtr;
                    mList->erase(j);
                    break;
                }
            }
        }
    }
    
    string locationForMeth(ObjCMethodDecl *meth) {
        auto &sm = compilerInstance.getSourceManager();
        string loc = meth->getLocation().printToString(sm);
        return loc;
    }
    
    void dump() {
        Array selArray = Array();
        for (auto i = undirectableSeletorRefList.begin(), e = undirectableSeletorRefList.end(); i != e ; i++) {
            selArray.push_back(Value(i->first));
        }
        
        Array overrideArray = Array();
        for (auto i = undirectableReadableList.begin(), e = undirectableReadableList.end(); i != e ; i++) {
            overrideArray.push_back(Value(*i));
        }

        Array methArray = Array();
        for (auto i = storage.begin(), e = storage.end(); i != e ; i++) {
            auto list = i->second;
            for (auto j = list->begin(), k = list->end(); j != k ; j++) {
                auto meth = *j;
                Object methInfoObj = Object();
                methInfoObj.insert({"name", meth->getFullName()});
                methInfoObj.insert({"sel", meth->getSelectorName()});
                methInfoObj.insert({"loc", meth->getFirstDeclLocation()});
                methInfoObj.insert({"isPropertyAccessor", meth->isPropertyAccessor});
                methArray.push_back(Value(Object(methInfoObj)));
            }
        }
         
        Object root = Object();
        root.insert({"sels", Array(selArray)});
        root.insert({"meths", Array(methArray)});
        root.insert({"undirect_meths", Array(overrideArray)});
        string content = llvm::formatv("{0:2}", Value(Object(root)));
        string path = string(<#give me a directory path to store results#>) + name + to_string(llvm::MD5Hash(content)) + string(".json");
        error_code ec = error_code();
        llvm::raw_fd_ostream *out = new llvm::raw_fd_stream(path, ec);
        
        *out << content;
        out->flush();
    }
};

class MethVisitor : public RecursiveASTVisitor<MethVisitor> {
    DirectableRecorder &recorder;
public:
    MethVisitor(DirectableRecorder &m) : recorder(m) {}
    
    
    // @selector(meth),
    // meth can't be marked as direct since msg reciver is undetermined
    bool VisitObjCSelectorExpr(ObjCSelectorExpr *E) {
        recorder.insertToUndirectableSel(E->getSelector());
        return true;
    }
    
    // protocl selector can't be direct
    // 'objc_direct' attribute cannot be applied to methods declared in an Objective-C protocol
    bool VisitObjCProtocolDecl(const ObjCProtocolDecl *D) {
        for (auto method = D->meth_begin(), methodEnd = D->meth_end(); method != methodEnd; method++) {
            recorder.insertToUndirectableSel(method->getSelector());
        }
        return true;
    }
    
    bool VisitObjCMessageExpr(const ObjCMessageExpr *OME) {
        // [(id)obj message];
        // [clz message];
        if (OME->getReceiverType()->isObjCIdType() || OME->getReceiverType()->isObjCClassType()) {
            recorder.insertToUndirectableSel(OME->getSelector());
            return true;
        }
        return true;
    }
    
    bool VisitObjCImplementationDecl(ObjCImplementationDecl *impDecl) {
        auto impClassInterface = impDecl->getClassInterface();
        recorder.name = impClassInterface->getNameAsString();
        
        // some override property that getter and setter is provider by superclass,
        // then we can't find getter and setter in subclass's implementation file
        // `dynmaic propertyName;` , so we fetch it from interface
        for (auto method = impClassInterface->meth_begin(), methodEnd = impClassInterface->meth_end(); method != methodEnd; method++) {
            auto methodDecl = *method;
            handleMeth(methodDecl, impClassInterface);
        }
        
        for (auto method = impDecl->meth_begin(), methodEnd = impDecl->meth_end(); method != methodEnd; method++) {
            auto methodDecl = *method;
            handleMeth(methodDecl, impClassInterface);
        }
        return true;
    }
    
    bool VisitObjCCategoryImplDecl(ObjCCategoryImplDecl *D) {
        auto categoryDecl = D->getCategoryDecl();
        auto impClassInterface = D->getClassInterface();
        auto name = impClassInterface->getNameAsString();
        recorder.name = name + "+" + categoryDecl->getNameAsString();
        
        for (auto method = D->meth_begin(), methodEnd = D->meth_end(); method != methodEnd; method++) {
            if (method->isDirectMethod()) continue;
            if (method->isOverriding()) continue;
            
            auto p = method->findPropertyDecl();
            if (p) {
                recorder.insertDirectablePropertyGetterMethod(impClassInterface, p);
            } else {
                // ERROR: `Direct method was declared in an extension but is implemented in a different category`
                ObjCPropertyDecl *propertyDeclHit = NULL;
                ObjCCategoryDecl *categoryDeclHit = NULL;
                bool metThisError = findDeclInExtButImpInDiffCategory(*method, impClassInterface, &propertyDeclHit, &categoryDeclHit);
                
                if (metThisError) {
                    // this property can't be direct
                    auto getterName = Tool::generateReadableName(categoryDeclHit, propertyDeclHit->getGetterMethodDecl());
                    recorder.insertUndirectableMethodName(getterName);
                } else {
                    // otherwise, it can
                    auto methInCategoryHeader = categoryDecl->getMethod(method->getSelector(), method->isInstanceMethod());
                    if (methInCategoryHeader) {
                        auto propertyDecl = methInCategoryHeader->findPropertyDecl();
                        if (propertyDecl) {
                            recorder.insertDirectablePropertyGetterMethod(categoryDecl, propertyDecl);
                        } else {
                            recorder.insertDirectableMethod(categoryDecl, methInCategoryHeader);
                        }
                    } else {
                        // case: primary class subclassing hook method is replaced by category
                        ObjCInterfaceDecl *interfaceDecl = NULL;
                        ObjCCategoryDecl *categoryDecll = NULL;
                        findFirstDeclPosition(*method, &interfaceDecl, &categoryDecll);
                        
                        // not exist in the category header but primary class/superclass' header, it's undirectable
                        if (interfaceDecl || categoryDecll) {
                            string name = generateName(*method, interfaceDecl, categoryDecl);
                            recorder.insertUndirectableMethodName(name);
                        } else {
                            recorder.insertDirectableMethod(categoryDecl, *method);
                        }
                    }
                }
            }
        }
        return true;
    }
    
private:
    bool findDeclInExtButImpInDiffCategory(ObjCMethodDecl *method,  ObjCInterfaceDecl *impClassInterface, ObjCPropertyDecl **propertyDeclHitPtr, ObjCCategoryDecl **categoryDeclHitPtr) {
        bool metThisError = false;
        auto methName = method->getNameAsString();
        auto category = impClassInterface->visible_categories().begin();
        auto categoryEnd = impClassInterface->visible_categories().end();
        while (category != categoryEnd) {
            auto categoryName = category->getNameAsString();
            auto cat = *category;
            for (auto pro = cat->prop_begin(), proEnd = cat->prop_end(); pro != proEnd; pro++) {
                auto proDecl = *pro;
                auto getter = proDecl->getGetterMethodDecl();
                if (!metThisError && getter) {
                    auto getterName = getter->getNameAsString();
                    if (methName == getterName) {
                        *propertyDeclHitPtr = proDecl;
                        *categoryDeclHitPtr = cat;
                        metThisError = true;
                        break;
                    }
                }
            
                auto setter = proDecl->getSetterMethodDecl();
                if (!metThisError && setter) {
                    auto setterName = setter->getNameAsString();
                    if (metThisError == false && methName == setterName) {
                        *propertyDeclHitPtr = proDecl;
                        *categoryDeclHitPtr = cat;
                        metThisError = true;
                        break;
                    }
                }
            }
            if (metThisError) break;
            category++;
        }
        return metThisError;
    }
    
    string generateName(const ObjCMethodDecl *methodDecl, ObjCInterfaceDecl *interfaceDecl, ObjCCategoryDecl *categoryDecl) {
        if ((!interfaceDecl && !categoryDecl) || !methodDecl) {
            return "";
        }
        
        string name;
        auto sel = methodDecl->getSelector();
        auto isInstance = methodDecl->isInstanceMethod();
        if (isUserSourceDecl(interfaceDecl)) {
            auto primaryMethDecl = interfaceDecl->getMethod(sel, isInstance);
            if (!primaryMethDecl) { // on interface's implementation
                primaryMethDecl = interfaceDecl->getImplementation()->getMethod(sel, isInstance);
            }
            
            if (primaryMethDecl->isPropertyAccessor()) {
                // use getter name as property identifier
                auto p = primaryMethDecl->findPropertyDecl();
                name = Tool::generateReadableName(interfaceDecl, p->getGetterMethodDecl());
            } else {
                name = Tool::generateReadableName(interfaceDecl, primaryMethDecl);
            }
        } else if (isUserSourceDecl(categoryDecl)) {
            auto categoryMethDecl = categoryDecl->getMethod(sel, isInstance);
            if (categoryMethDecl && categoryMethDecl->isPropertyAccessor()) {
                auto p = categoryMethDecl->findPropertyDecl();
                name = Tool::generateReadableName(categoryDecl, p->getGetterMethodDecl());
            } else {
                name = Tool::generateReadableName(categoryDecl, categoryMethDecl);
            }
        }
        return name;
    }

    void handleMeth(ObjCMethodDecl *methodDecl, ObjCInterfaceDecl *passInInterfaceDecl) {
        llvm::outs() << "LOC:  " << methodDecl->getLocation().printToString(recorder.getCompilerInstance().getSourceManager()) << "\n";
        auto methodName = methodDecl->getNameAsString();
        llvm::outs() << "NAME: " << methodName << "\n";
        
        if (methodDecl->isDirectMethod()) return;
        if (methodName == ".cxx_destruct") return;
        
        Selector sel = methodDecl->getSelector();
        bool isInstance = methodDecl->isInstanceMethod();
        
        // doesn't work
//        if (methodDecl->getReturnType()->hasAttr(attr::IBAction)) return false;
        
        
        ObjCInterfaceDecl *interfaceDecl = NULL;
        ObjCCategoryDecl *categoryDecl = NULL;
        findFirstDeclPosition(methodDecl, &interfaceDecl, &categoryDecl);
        
        
        // overriding decl, exist in super class/super class category, protocol
        if (methodDecl->isOverriding()) {
            string name = generateName(methodDecl, interfaceDecl, categoryDecl);
            recorder.insertUndirectableMethodName(name);
        // override
        } else {
        // not override
            ObjCInterfaceDecl *currentInterface = passInInterfaceDecl;
            
            // found in superclass/superclass-category
            if ((interfaceDecl && currentInterface != interfaceDecl) || (categoryDecl && currentInterface != categoryDecl->getClassInterface())) {
                // case:
                // class A               -height
                // class B: class A      @height
                string name = generateName(methodDecl, interfaceDecl, categoryDecl);
                recorder.insertUndirectableMethodName(name);
                return;
            }
            
            ObjCMethodDecl *firstDecl = NULL;
            bool existInHeader = false;
            if (interfaceDecl) {
                firstDecl = interfaceDecl->getMethod(sel, isInstance);
                if (!firstDecl) {
                    firstDecl = interfaceDecl->getImplementation()->getMethod(sel, isInstance);
                } else {
                    existInHeader = true;
                }
            } else if (categoryDecl) {
                firstDecl = categoryDecl->getMethod(sel, isInstance);
                existInHeader = true;
            } else {
                firstDecl = methodDecl;
            }
            
            if (existInHeader) {
                auto ppp = firstDecl->findPropertyDecl();
                if (ppp) {
                    auto pSetter = ppp->getSetterMethodDecl();
                    auto pGetter = ppp->getGetterMethodDecl();

                    ObjCInterfaceDecl *pinterfaceDecl = NULL;
                    ObjCCategoryDecl *pcategoryDecl = NULL;
                    if (pSetter) { // setter search
                        findFirstDeclPosition(pSetter, &pinterfaceDecl, &pcategoryDecl);
                        bool superHasDecl = (pinterfaceDecl && pSetter->getClassInterface() != pinterfaceDecl);
                        superHasDecl = superHasDecl || (pcategoryDecl && pcategoryDecl->getClassInterface() != pSetter->getClassInterface());
                        if (pGetter && superHasDecl) {
                            string name = generateName(pGetter, pinterfaceDecl, pcategoryDecl);
                            recorder.insertUndirectableMethodName(name);
                            return;
                        }
                        
                        // ok let's skip setter
                        if (pSetter->getNameAsString() == methodDecl->getNameAsString()) {
                            return;
                        }
                    }
                    
                    pinterfaceDecl = NULL;
                    pcategoryDecl = NULL;
                    if (pGetter) { // getter search
                        findFirstDeclPosition(pGetter, &pinterfaceDecl, &pcategoryDecl);
                        bool superHasDecl = (pinterfaceDecl && pGetter->getClassInterface() != pinterfaceDecl);
                        superHasDecl = superHasDecl || (pcategoryDecl && pcategoryDecl->getClassInterface() != pGetter->getClassInterface());
                        if (pGetter && superHasDecl) {
                            string name = generateName(pGetter, pinterfaceDecl, pcategoryDecl);
                            recorder.insertUndirectableMethodName(name);
                            return;
                        }
                    }
                }
            }
            
            if (interfaceDecl) {
                recorder.insertDirectableMethod(interfaceDecl, firstDecl);
            } else if (categoryDecl) {
                recorder.insertDirectableMethod(categoryDecl, firstDecl);
            } else {
                recorder.insertDirectableMethod(firstDecl->getClassInterface(), firstDecl);
            }
        }
    }
    
    // find the very first declaration postion
    void findFirstDeclPosition(const ObjCMethodDecl *methodDecl, ObjCInterfaceDecl **interfaceDeclPtr, ObjCCategoryDecl **categoryDeclPtr) {

        bool searchWholePath = true;

        ObjCInterfaceDecl *interfaceDecl = (ObjCInterfaceDecl *)methodDecl->getClassInterface();
        ObjCCategoryDecl *categoryDecl = NULL;
        bool classDeclFound = false;
        bool categoryDeclFound = false;
        ObjCInterfaceDecl *primaryClassCursor = interfaceDecl;
        
        Selector sel = methodDecl->getSelector();
        bool isInstance = methodDecl->isInstanceMethod();

        while (primaryClassCursor != NULL) {
            auto primaryMethDecl = primaryClassCursor->getMethod(sel, isInstance);
            
            ObjCMethodDecl *methInImp = NULL;
            if (!primaryMethDecl && primaryClassCursor != interfaceDecl) { // superclass' implementation
                auto cursorClzImp = primaryClassCursor->getImplementation();
                if (cursorClzImp) {
                    methInImp = cursorClzImp->getMethod(sel, isInstance);
                }
            }
            
            if (primaryMethDecl) { // class inherited chain
                interfaceDecl = primaryClassCursor;
                classDeclFound = true;
                categoryDeclFound = false;
                if (!searchWholePath) break;
            } else if (methInImp) { // class inherited chain's implementation
                interfaceDecl = primaryClassCursor;
                classDeclFound = true;
                categoryDeclFound = false;
                if (!searchWholePath) break;
            } else {
                // class inherited chain's category
                auto begin = primaryClassCursor->visible_categories().begin(), end = primaryClassCursor->visible_categories().end();
                while (begin != end) {
                    auto category = *begin;
                    auto categoryMethDecl = category->getMethod(sel, isInstance);
                    if (categoryMethDecl) {
                        categoryDecl = category;
                        categoryDeclFound = true;
                        classDeclFound = false;
                        break;
                    }
                    begin++;
                }
                if (!searchWholePath) {
                    if (categoryDeclFound) break;
                }
            }
            primaryClassCursor = primaryClassCursor->getSuperClass();
        }
        
        if (classDeclFound) {
            *interfaceDeclPtr = interfaceDecl;
        }

        if (categoryDeclFound) {
            *categoryDeclPtr = categoryDecl;
        }
    }

    bool isUserSourceDecl(const Decl *decl) {
        if (!decl) return false;
        string filename = recorder.getCompilerInstance().getSourceManager().getFilename(decl->getLocation()).str();
        if (filename.empty()) return false;
        return (filename.find("/Applications/Xcode") != 0);
    }
};

class DFConsumer : public ASTConsumer {
    CompilerInstance &compilerInstance;
public:
    DFConsumer(CompilerInstance &CI) : compilerInstance(CI) {}
    
    // 处理 TranslationUnit
    void HandleTranslationUnit(ASTContext &Ctx) override {
        DirectableRecorder recorder(compilerInstance);
        MethVisitor v(recorder);
        TranslationUnitDecl *unit = Ctx.getTranslationUnitDecl();
        v.TraverseDecl(unit);
    }
};

class DFAction : public PluginASTAction {
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
        return std::make_unique<DFConsumer>(CI);
    }

    bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
        return true;
    }
};


static FrontendPluginRegistry::Add<DFAction>
X("directable-finder", "description");
