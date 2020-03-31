//===----- Genode.h - Genode ToolChain Implementations ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_GENODE_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_GENODE_H

#include "Gnu.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace tools {

//// genode -- Directly call GNU Binutils linker
namespace genode {
class LLVM_LIBRARY_VISIBILITY Linker : public GnuTool {
public:
  Linker(const ToolChain &TC) : GnuTool("genode::Linker", "linker", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputss,
                    const llvm::opt::ArgList &Args,
                    const char *LinkingOutput) const override;
};
} // end namespace genode
} // end namespace tools

namespace toolchains {

class LLVM_LIBRARY_VISIBILITY Genode : public Generic_ELF {
public:
  Genode(const Driver &D, const llvm::Triple &Triple,
         const llvm::opt::ArgList &Args);
  bool HasNativeLLVMSupport() const override { return true; }

  bool IsMathErrnoDefault() const override { return true; }

  CXXStdlibType
  GetCXXStdlibType(const llvm::opt::ArgList &Args) const override {
    return ToolChain::CST_Libcxx;
  }
  void addLibCxxIncludePaths(
      const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;

  bool isPIEDefault() const override;
  SanitizerMask getSupportedSanitizers() const override;
  SanitizerMask getDefaultSanitizers() const override;

protected:
  Tool *buildLinker() const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_GENODE_H
