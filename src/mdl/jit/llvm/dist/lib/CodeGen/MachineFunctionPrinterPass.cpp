//===-- MachineFunctionPrinterPass.cpp ------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// MachineFunctionPrinterPass implementation.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
/// MachineFunctionPrinterPass - This is a pass to dump the IR of a
/// MachineFunction.
///
struct MachineFunctionPrinterPass : public MachineFunctionPass {
  static char ID;

  raw_ostream &OS;
  const MISTD::string Banner;

  MachineFunctionPrinterPass() : MachineFunctionPass(ID), OS(dbgs()) { }
  MachineFunctionPrinterPass(raw_ostream &os, const MISTD::string &banner) 
      : MachineFunctionPass(ID), OS(os), Banner(banner) {}

  const char *getPassName() const { return "MachineFunction Printer"; }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) {
    OS << "# " << Banner << ":\n";
    MF.print(OS, getAnalysisIfAvailable<SlotIndexes>());
    return false;
  }
};

char MachineFunctionPrinterPass::ID = 0;
}

char &llvm::MachineFunctionPrinterPassID = MachineFunctionPrinterPass::ID;
INITIALIZE_PASS(MachineFunctionPrinterPass, "print-machineinstrs",
                "Machine Function Printer", false, false)

namespace llvm {
/// Returns a newly-created MachineFunction Printer pass. The
/// default banner is empty.
///
MachineFunctionPass *createMachineFunctionPrinterPass(raw_ostream &OS,
                                                      const MISTD::string &Banner){
  return new MachineFunctionPrinterPass(OS, Banner);
}

}