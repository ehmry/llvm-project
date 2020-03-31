//===--- Genode.h - Genode ToolChain Implementations ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Genode.h"
#include "CommonArgs.h"
#include "InputInfo.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

void genode::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                  const InputInfo &Output,
                                  const InputInfoList &Inputs,
                                  const ArgList &Args,
                                  const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  const auto &TC = static_cast<const toolchains::Genode &>(getToolChain());
  const auto &D = TC.getDriver();

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  // Explicitly set the linker emulation for platforms that might not
  // be the default emulation for the linker.
  switch (TC.getArch()) {
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
  TC.AddFilePathLibArgs(Args, CmdArgs);
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

  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs, options::OPT_noposix)) {
    AddRunTimeLibs(TC, D, CmdArgs, Args);

    CmdArgs.push_back("-lc");
    if (!Args.hasArg(options::OPT_shared)) {
      CmdArgs.push_back("-lposix");
    }
  }

  C.addCommand(std::make_unique<Command>(JA, *this, ResponseFileSupport::None(),
                                         Args.MakeArgString(TC.GetLinkerPath()),
                                         CmdArgs, Inputs));
}

Genode::Genode(const Driver &D, const llvm::Triple &Triple,
               const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {

  std::vector<std::string> GenodeExtraTriples;
  switch (Triple.getArch()) {
  case llvm::Triple::arm:
    GenodeExtraTriples.push_back("arm-none-eabi");
    break;
  case llvm::Triple::aarch64:
    GenodeExtraTriples.push_back("aarch64-none-elf");
    break;
  case llvm::Triple::x86:
  case llvm::Triple::x86_64:
    GenodeExtraTriples.push_back("x86_64-pc-elf");
    break;
  case llvm::Triple::riscv64:
    GenodeExtraTriples.push_back("riscv64-unknown-elf");
    break;
  default:
    break;
  }

  GCCInstallation.init(Triple, Args, GenodeExtraTriples);

  const std::string MultiarchTriple = getMultiarchTriple(D, Triple, /*SysRoot*/ "");
  if (GCCInstallation.isValid() &&
      (Triple.getArch() == llvm::Triple::x86_64 ||
       Triple.getArch() == llvm::Triple::x86)) {

    path_list MultilibPaths;
    Generic_GCC::AddMultilibPaths(D, /*SysRoot*/ "", "lib",
                                  MultiarchTriple, MultilibPaths);

    auto Suffix = GCCInstallation.getMultilib().gccSuffix();
    for (auto path: MultilibPaths)
      addPathIfExists(D, path + Suffix, getFilePaths());
  } else {
    Generic_GCC::AddMultilibPaths(D, /*SysRoot*/ "", "lib",
                                  MultiarchTriple, getFilePaths());
  }

  ToolChain::path_list &PPaths = getProgramPaths();

  Generic_GCC::PushPPaths(PPaths);

#ifdef ENABLE_LINKER_BUILD_ID
  ExtraOpts.push_back("--build-id");
#endif
}

void
Genode::addLibStdCxxIncludePaths(const llvm::opt::ArgList &DriverArgs,
                                 llvm::opt::ArgStringList &CC1Args) const {
  // Try generic GCC detection first.
  if (Generic_GCC::addGCCLibStdCxxIncludePaths(DriverArgs, CC1Args))
    return;

  if (!GCCInstallation.isValid())
    return;

  StringRef LibDir = GCCInstallation.getParentLibPath();
  StringRef TripleStr = GCCInstallation.getTriple().str();
  const Multilib &Multilib = GCCInstallation.getMultilib();
  const GCCVersion &Version = GCCInstallation.getVersion();

  const std::string IncludePath = {
      LibDir.str() + "/../" + TripleStr.str() + "/include/c++/" + Version.Text,
  };

  addLibStdCXXIncludePaths(IncludePath, /*Suffix*/ "", TripleStr,
                           /*GCCMultiarchTriple*/ "",
                           /*TargetMultiarchTriple*/ "",
                           Multilib.includeSuffix(), DriverArgs, CC1Args);
}

void Genode::addExtraOpts(llvm::opt::ArgStringList &CmdArgs) const {
  for (const auto &Opt : ExtraOpts)
    CmdArgs.push_back(Opt.c_str());
}

Tool *Genode::buildLinker() const {
  return new tools::genode::Linker(*this);
}
