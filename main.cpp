#include <cmath>

#include <binaryninjaapi.h>
#include <lowlevelilinstruction.h>

using namespace BinaryNinja;

const uint64_t MIN_STACK_ALIGN = 4;

void AlignFixer(Ref<AnalysisContext> analysisContext) {
  Ref<LowLevelILFunction> llilFunction =
      analysisContext->GetLowLevelILFunction();

  if (!llilFunction)
    return;

  bool updated = false;
  for (auto &bb : llilFunction->GetBasicBlocks()) {
    Ref<Architecture> arch = bb->GetArchitecture();
    auto stackPointerRegister = arch->GetStackPointerRegister();
    uint64_t stackPointerRegisterSizeMask =
        std::pow(2, arch->GetRegisterInfo(stackPointerRegister).size * 8) - 1;

    for (size_t instrIndex = bb->GetStart(); instrIndex < bb->GetEnd();
         instrIndex++) {
      LowLevelILInstruction instruction =
          llilFunction->GetInstruction(instrIndex);

      if (instruction.operation != LLIL_SET_REG)
        continue;

      if (instruction.GetDestRegister() != stackPointerRegister)
        continue;

      auto sourceExpr = instruction.GetSourceExpr<LLIL_SET_REG>();
      if (sourceExpr.operation != LLIL_AND)
        continue;

      auto andLeftExpr = sourceExpr.GetLeftExpr<LLIL_AND>();
      if (andLeftExpr.operation != LLIL_REG)
        continue;

      if (andLeftExpr.GetSourceRegister() != stackPointerRegister)
        continue;

      auto andRightExpr = sourceExpr.GetRightExpr<LLIL_AND>();
      if (andRightExpr.operation != LLIL_CONST)
        continue;

      auto alignMask = andRightExpr.GetConstant();
      bool isAligned = !(((~alignMask & stackPointerRegisterSizeMask) + 1) %
                         MIN_STACK_ALIGN);

      if (!isAligned)
        continue;

      // LogDebug("%08lx %s %d", instruction.address,
      //         arch->GetRegisterName(instruction.GetDestRegister()).c_str(),
      //         isAligned);

      // It's a stack align, kill it
      instruction.Replace(llilFunction->Nop());
      updated = true;
    }
  }

  if (updated)
    llilFunction->GenerateSSAForm();
}

extern "C" {
BN_DECLARE_CORE_ABI_VERSION

BINARYNINJAPLUGIN bool CorePluginInit() {
  Ref<Workflow> alignFixWorkflow =
      Workflow::Instance()->Clone("AlignFixWorkflow");
  alignFixWorkflow->RegisterActivity(
      new Activity("extension.alignFix", &AlignFixer));
  alignFixWorkflow->Insert("core.function.translateTailCalls",
                           "extension.alignFix");
  Workflow::RegisterWorkflow(alignFixWorkflow,
                             R"#({
      "title" : "Stack Align Fix",
      "description" : "Removes instructions that align the stack pointer to fix analysis",
      "capabilities" : []
    })#");

  return true;
}
}
