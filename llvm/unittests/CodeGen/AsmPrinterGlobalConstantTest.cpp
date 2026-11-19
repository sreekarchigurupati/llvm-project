//===- llvm/unittest/CodeGen/AsmPrinterGlobalConstantTest.cpp ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TestAsmPrinter.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Testing/Support/Error.h"

using namespace llvm;
using testing::InSequence;

namespace {

class AsmPrinterGlobalConstantTest : public testing::Test {
protected:
  static void SetUpTestSuite() {
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmPrinters();
  }

  /// Create a printer for a module with the data layout \p DataLayoutStr.
  /// Return false if the target is not available.
  bool init(StringRef DataLayoutStr) {
    Expected<std::unique_ptr<TestAsmPrinter>> ExpectedTestPrinter =
        TestAsmPrinter::create("x86_64-pc-linux", /*DwarfVersion=*/4,
                               dwarf::DWARF32);
    EXPECT_THAT_EXPECTED(ExpectedTestPrinter, Succeeded());
    if (!ExpectedTestPrinter)
      return false;
    TestPrinter = std::move(*ExpectedTestPrinter);
    if (!TestPrinter)
      return false;

    M = std::make_unique<Module>("AsmPrinterGlobalConstantTest", Ctx);
    M->setDataLayout(DataLayoutStr);
    AsmPrinter *AP = TestPrinter->getAP();
    MMIWP = std::make_unique<MachineModuleInfoWrapperPass>(&AP->TM);
    MMIWP->doInitialization(*M);
    AP->MMI = &MMIWP->getMMI();
    return true;
  }

  void emitNullPointer(unsigned AddrSpace) {
    TestPrinter->getAP()->emitGlobalConstant(
        M->getDataLayout(),
        ConstantPointerNull::get(PointerType::get(Ctx, AddrSpace)));
  }

  LLVMContext Ctx;
  std::unique_ptr<TestAsmPrinter> TestPrinter;
  std::unique_ptr<Module> M;
  std::unique_ptr<MachineModuleInfoWrapperPass> MMIWP;
};

TEST_F(AsmPrinterGlobalConstantTest, NullPointer) {
  // Address space 1 has an all-ones null pointer. Address spaces 2, 3, and 4
  // have pointers wider than 64 bits. The 160-bit pointers of address space 4
  // have 20 bytes of data and 12 bytes of padding.
  if (!init("e-p:64:64-po1:32:32-p2:128:128-po3:128:128-"
            "po4:160:256:256:32"))
    GTEST_SKIP();

  MockMCStreamer &MS = TestPrinter->getMS();
  InSequence S;

  EXPECT_CALL(MS, emitIntValue(0, 8));
  emitNullPointer(0);

  EXPECT_CALL(MS, emitIntValue(0xFFFFFFFF, 4));
  emitNullPointer(1);

  EXPECT_CALL(MS, emitIntValue(0, 8)).Times(2);
  emitNullPointer(2);

  EXPECT_CALL(MS, emitIntValue(UINT64_MAX, 8)).Times(2);
  emitNullPointer(3);

  EXPECT_CALL(MS, emitIntValue(UINT64_MAX, 8)).Times(2);
  EXPECT_CALL(MS, emitIntValue(0xFFFFFFFF, 4));
  emitNullPointer(4);
}

} // end anonymous namespace
