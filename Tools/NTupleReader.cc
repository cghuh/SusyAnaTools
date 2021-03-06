#include "NTupleReader.h"

#include "TROOT.h"
#include "TInterpreter.h"
#include "TObjArray.h"
#include "TLeaf.h"

NTupleReader::NTupleReader(TTree * tree, std::set<std::string>& activeBranches) : activeBranches_(activeBranches)
{
    tree_ = tree;
    if(!tree_) THROW_SATEXCEPTION("NTupleReader(...): TTree " + std::string(tree_->GetName()) + " is invalid!!!!");
    init();
}

NTupleReader::NTupleReader(TTree * tree)
{
    tree_ = tree;
    init();
}

void NTupleReader::init()
{
    nevt_ = 0;
    isUpdateDisabled_ = false;
    isFirstEvent_ = true;
    reThrow_ = true;

    activateBranches();    
}


// ===  FUNCTION  ============================================================
//         Name:  NTupleReader::GetCurrentInfo
//  Description:  Useful function for debuging
// ===========================================================================
bool NTupleReader::GetCurrentInfo()
{
  std::cout << "In Run " << getVar<int>("run") << " event " << getVar<int>("event")  << " from file: "
    << tree_->GetCurrentFile()->GetName()<< std::endl;
  return true;
}       // -----  end of function NTupleReader::GetCurrentInfo  -----

void NTupleReader::activateBranches()
{
    tree_->SetBranchStatus("*", 0);

    // Add desired branches to branchMap_
    populateBranchList();

    for(auto& iMap : branchMap_)
    {
        tree_->SetBranchStatus(iMap.first.c_str(), 1);
        tree_->SetBranchAddress(iMap.first.c_str(), iMap.second);
    }

    for(auto& iMap : branchVecMap_)
    {
        tree_->SetBranchStatus(iMap.first.c_str(), 1);
        tree_->SetBranchAddress(iMap.first.c_str(), iMap.second);
    }
}

void NTupleReader::populateBranchList()
{
    TObjArray *lob = tree_->GetListOfBranches();
    TIter next(lob);
    TBranch *branch;

    while(branch = (TBranch*)next()) 
    {
        std::string name(branch->GetName());

        if(activeBranches_.size() > 0 && activeBranches_.count(name) == 0)
        {
            //allow typeMap_ to track that the branch exists without filling type
            typeMap_[name] = "";
            continue;
        }

        registerBranch(branch);
    }
}

void NTupleReader::registerBranch(TBranch * const branch) const
{
    std::string type(branch->GetTitle());
    std::string name(branch->GetName());

    if(type.compare(name) == 0)
    {
        TObjArray *lol = branch->GetListOfLeaves();
        if (lol->GetEntries() == 1) 
        {
            TLeaf *leaf = (TLeaf*)lol->UncheckedAt(0);
            type = leaf->GetTypeName();
        }
        //else continue;
    }

    if(type.find("vector") != std::string::npos)
    {
        if     (type.find("double")         != std::string::npos) registerVecBranch<double>(name);
        else if(type.find("unsigned int")   != std::string::npos) registerVecBranch<unsigned int>(name);
        else if(type.find("int")            != std::string::npos) registerVecBranch<int>(name);
        else if(type.find("string")         != std::string::npos) registerVecBranch<std::string>(name);
        else if(type.find("TLorentzVector") != std::string::npos) registerVecBranch<TLorentzVector>(name);
        else if(type.find("float")          != std::string::npos) registerVecBranch<float>(name);
        else THROW_SATEXCEPTION("No type match for branch \"" + name + "\" with type \"" + type + "\"!!!");
    }
    else
    {
        if     (type.find("/D") != std::string::npos) registerBranch<double>(name);
        else if(type.find("/I") != std::string::npos) registerBranch<int>(name);
        else if(type.find("/i") != std::string::npos) registerBranch<unsigned int>(name);
        else if(type.find("/F") != std::string::npos) registerBranch<float>(name);
        else if(type.find("/C") != std::string::npos) registerBranch<char>(name);
        else if(type.find("/c") != std::string::npos) registerBranch<unsigned char>(name);
        else if(type.find("/S") != std::string::npos) registerBranch<short>(name);
        else if(type.find("/s") != std::string::npos) registerBranch<unsigned short>(name);
        else if(type.find("/O") != std::string::npos) registerBranch<bool>(name);
        else if(type.find("/L") != std::string::npos) registerBranch<unsigned long>(name);
        else if(type.find("/l") != std::string::npos) registerBranch<long>(name);
        else THROW_SATEXCEPTION("No type match for branch \"" + name + "\" with type \"" + type + "\"!!!");
    }
}

bool NTupleReader::getNextEvent()
{
    int status = tree_->GetEntry(nevt_);
    if (status == 0) return false;
    nevt_++;
    if(nevt_ >= 2) isFirstEvent_ = false;
    calculateDerivedVariables();
    return status > 0;
}

void NTupleReader::disableUpdate()
{
    isUpdateDisabled_ = true;
    printf("NTupleReader::disableUpdate(): You have disabled tuple updates.  You may therefore be using old variablre definitions.  Be sure you are ok with this!!!\n");
}

void NTupleReader::calculateDerivedVariables()
{
    for(auto& func : functionVec_)
    {
        func(*this);
    }
}

void NTupleReader::registerFunction(void (*f)(NTupleReader&))
{
    if(isFirstEvent_) functionVec_.emplace_back(f);
    else printf("NTupleReader::registerFunction(...): new functions cannot be registered after tuple reading begins!\n");
}

void NTupleReader::getType(const std::string& name, std::string& type) const
{
    auto typeIter = typeMap_.find(name);
    if(typeIter != typeMap_.end())
    {
        type = typeIter->second;
    }
}

void NTupleReader::setReThrow(const bool reThrow)
{
    reThrow_ = reThrow;
}

bool NTupleReader::getReThrow() const
{
    return reThrow_;
}

const void* NTupleReader::getPtr(const std::string var) const
{
    //This function can be used to return the variable pointer
    try
    {
        auto tuple_iter = branchMap_.find(var);
        if(tuple_iter != branchMap_.end())
        {
            return tuple_iter->second;
        }

        THROW_SATEXCEPTION("NTupleReader::getPtr(...): Variable not found: " + var);
    }
    catch(const SATException& e)
    {
        e.print();
        if(reThrow_) throw;
    }
}

const void* NTupleReader::getVecPtr(const std::string var) const
{
    //This function can be used to return the variable pointer

    try
    {
        auto tuple_iter = branchVecMap_.find(var);
        if(tuple_iter != branchVecMap_.end())
        {
            return tuple_iter->second;
        }

        THROW_SATEXCEPTION("NTupleReader::getVecPtr(...): Variable not found: " + var);
    }
    catch(const SATException& e)
    {
        e.print();
        if(reThrow_) throw;
    }
}

void NTupleReader::printTupleMembers(FILE *f) const
{
    for(auto& iVar : typeMap_)
    {
        fprintf(f, "%60s %s\n", iVar.second.c_str(), iVar.first.c_str());
    }
}

std::vector<std::string> NTupleReader::GetTupleMembers() const
{
  std::vector<std::string> members;
  for(auto& iVar : typeMap_)
  {
    members.push_back(iVar.first);
  }
  return members;
}


std::vector<std::string> NTupleReader::GetTupleSpecs(std::string VarName) const
{
  std::vector<std::string> members = GetTupleMembers();
  std::vector<std::string> specs;
  for(auto &member : members)
  {
    std::string::size_type t= member.find(VarName);
    if (t != std::string::npos)
    {
      specs.push_back(member.erase(t, VarName.length()));
    }
  }
  
  return specs;
}

// ===  FUNCTION  ============================================================
//         Name:  NTupleReader::HasVar
//  Description:  
// ===========================================================================
bool NTupleReader::hasVar(std::string name) const
{
  return (typeMap_.find(name) != typeMap_.end());
}       // -----  end of function NTupleReader::HasVar  -----
