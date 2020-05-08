//===--- Genode.cpp - Genode ToolChain Implementations ----------*- C++ -*-===//
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
  const toolchains::Genode &ToolChain =
      static_cast<const toolchains::Genode &>(getToolChain());
  const Driver &D = ToolChain.getDriver();
  const llvm::Triple::ArchType Arch = ToolChain.getArch();
  ArgStringList CmdArgs;

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  // Explicitly set the linker emulation for platforms that might not
  // be the default emulation for the linker.
  switch (Arch) {
  case llvm::Triple::x86:
    CmdArgs.push_back("-melf_i386");
    break;
  case llvm::Triple::x86_64:
    CmdArgs.push_back("-melf_x86_64");
    break;
  case llvm::Triple::riscv32:
    CmdArgs.push_back("-melf32lriscv");
    break;
  case llvm::Triple::riscv64:
    CmdArgs.push_back("-melf64lriscv");
    break;
  default:
    break;
  }

  CmdArgs.push_back("--eh-frame-hdr");
  CmdArgs.push_back("--gc-sections");
  CmdArgs.push_back("-zmax-page-size=0x1000");

  CmdArgs.push_back("-Ttext=0x01000000");

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs, options::OPT_T_Group);
  Args.AddAllArgs(CmdArgs, options::OPT_e);
  Args.AddAllArgs(CmdArgs, options::OPT_s);
  Args.AddAllArgs(CmdArgs, options::OPT_t);
  Args.AddAllArgs(CmdArgs, options::OPT_Z_Flag);

  if (Args.hasArg(options::OPT_static)) {
    CmdArgs.push_back("-Bstatic");
  } else {
    if (Args.hasArg(options::OPT_shared)) {
      CmdArgs.push_back(Args.MakeArgString("-shared"));
      CmdArgs.push_back(Args.MakeArgString("-T" + D.SysRoot + "/ld/genode_rel.ld"));
    } else {
      CmdArgs.push_back(Args.MakeArgString("-T" + D.SysRoot + "/ld/genode_dyn.ld"));
      CmdArgs.push_back(Args.MakeArgString("--dynamic-list=" + D.SysRoot + "/ld/genode_dyn.dl"));
      CmdArgs.push_back("--dynamic-linker=ld.lib.so");
    }
    if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
      CmdArgs.push_back("-l:ld.lib.so");
    }
  }

  if (Output.isFilename()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());
  } else {
    assert(Output.isNothing() && "Invalid output.");
  }

  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs, options::OPT_noposix)) {
    CmdArgs.push_back("-lc");
    if (!Args.hasArg(options::OPT_shared)) {
      CmdArgs.push_back("-lposix");
    }
  }

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
