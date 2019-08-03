//===--- Genode.cpp - CloudABI ToolChain Implementations --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#include "Genode.h"
#include "InputInfo.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Options.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

void genode::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                  const InputInfo &Output,
                                  const InputInfoList &Inputs,
                                  const llvm::opt::ArgList &Args,
                                  const char *LinkingOutput) const  {
  const ToolChain &ToolChain = getToolChain();
  const Driver &D = ToolChain.getDriver();
  ArgStringList CmdArgs;

  if (Output.isFilename()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());
  } else {
    assert(Output.isNothing() && "Invalid output.");
  }

  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  const char *Exec = Args.MakeArgString(ToolChain.GetLinkerPath());
  C.addCommand(llvm::make_unique<Command>(JA, *this, Exec, CmdArgs, Inputs));
}

Genode::Genode(const Driver &D, const llvm::Triple &Triple,
               const llvm::opt::ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  SmallString<128> P(getDriver().Dir);
  llvm::sys::path::append(P, "..", getTriple().str(), "lib");
  getFilePaths().push_back(P.str());
}

void Genode::addLibCxxIncludePaths(const llvm::opt::ArgList &DriverArgs,
                                   llvm::opt::ArgStringList &CC1Args) const {
  SmallString<128> P(getDriver().Dir);
  llvm::sys::path::append(P, "..", getTriple().str(), "include/c++/v1");
  addSystemInclude(DriverArgs, CC1Args, P.str());
}

bool Genode::isPIEDefault() const {
  switch (getTriple().getArch()) {
  case llvm::Triple::aarch64:
  case llvm::Triple::x86_64:
    return true;
  default:
    return false;
  }
}

SanitizerMask Genode::getSupportedSanitizers() const {
  return Generic_ELF::getSupportedSanitizers();
}

SanitizerMask Genode::getDefaultSanitizers() const {
  return Generic_ELF::getDefaultSanitizers();
}

Tool *Genode::buildLinker() const {
  return new tools::genode::Linker(*this);
}
