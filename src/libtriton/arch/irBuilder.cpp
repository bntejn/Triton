//! \file
/*
**  Copyright (C) - Triton
**
**  This program is under the terms of the BSD License.
*/

#include <new>

#include <triton/exceptions.hpp>
#include <triton/irBuilder.hpp>
#include <triton/memoryAccess.hpp>
#include <triton/operandWrapper.hpp>
#include <triton/register.hpp>
#include <triton/x86Semantics.hpp>
#include <triton/astContext.hpp>



namespace triton {
  namespace arch {

    IrBuilder::IrBuilder(triton::arch::Architecture* architecture,
                         const triton::modes::Modes& modes,
                         triton::ast::AstContext& astCtxt,
                         triton::engines::symbolic::SymbolicEngine* symbolicEngine,
                         triton::engines::taint::TaintEngine* taintEngine)
      : modes(modes),
        astGarbageCollector(astCtxt.getAstGarbageCollector()),
        backupAstGarbageCollector(modes, true) {

      if (architecture == nullptr)
        throw triton::exceptions::IrBuilder("IrBuilder::IrBuilder(): The architecture API must be defined.");

      if (symbolicEngine == nullptr)
        throw triton::exceptions::IrBuilder("IrBuilder::IrBuilder(): The symbolic engine API must be defined.");

      if (taintEngine == nullptr)
        throw triton::exceptions::IrBuilder("IrBuilder::IrBuilder(): The taint engines API must be defined.");

      this->architecture              = architecture;
      this->backupSymbolicEngine      = new(std::nothrow) triton::engines::symbolic::SymbolicEngine(architecture, modes, astCtxt, nullptr, true);
      this->symbolicEngine            = symbolicEngine;
      this->taintEngine               = taintEngine;
      this->x86Isa                    = new(std::nothrow) triton::arch::x86::x86Semantics(architecture, symbolicEngine, taintEngine, astCtxt);

      if (this->x86Isa == nullptr || this->backupSymbolicEngine == nullptr)
        throw triton::exceptions::IrBuilder("IrBuilder::IrBuilder(): Not enough memory.");
    }


    IrBuilder::~IrBuilder() {
      delete this->backupSymbolicEngine;
      delete this->x86Isa;
    }


    bool IrBuilder::buildSemantics(triton::arch::Instruction& inst) {
      bool ret = false;

      if (this->architecture->getArchitecture() == triton::arch::ARCH_INVALID)
        throw triton::exceptions::IrBuilder("IrBuilder::buildSemantics(): You must define an architecture.");

      /* Stage 1 - Update the context memory */
      std::list<triton::arch::MemoryAccess>::iterator it1;
      for (it1 = inst.memoryAccess.begin(); it1 != inst.memoryAccess.end(); it1++) {
        this->architecture->setConcreteMemoryValue(*it1);
      }

      /* Stage 2 - Update the context register */
      std::map<triton::uint32, triton::arch::Register>::iterator it2;
      for (it2 = inst.registerState.begin(); it2 != inst.registerState.end(); it2++) {
        this->architecture->setConcreteRegisterValue(it2->second);
      }

      /* Stage 3 - Initialize the target address of memory operands */
      std::vector<triton::arch::OperandWrapper>::iterator it3;
      for (it3 = inst.operands.begin(); it3 != inst.operands.end(); it3++) {
        if (it3->getType() == triton::arch::OP_MEM) {
          this->symbolicEngine->initLeaAst(it3->getMemory());
        }
      }

      /* Pre IR processing */
      this->preIrInit(inst);

      /* Processing */
      switch (this->architecture->getArchitecture()) {
        case triton::arch::ARCH_X86:
        case triton::arch::ARCH_X86_64:
          ret = this->x86Isa->buildSemantics(inst);
      }

      /* Post IR processing */
      this->postIrInit(inst);

      return ret;
    }


    void IrBuilder::preIrInit(triton::arch::Instruction& inst) {
      /* Clear previous expressions if exist */
      inst.symbolicExpressions.clear();

      /* Update instruction address if undefined */
      if (!inst.getAddress())
        inst.setAddress(this->architecture->getConcreteRegisterValue(this->architecture->getParentRegister(ID_REG_IP)).convert_to<triton::uint64>());

      /* Backup the symbolic engine in the case where only the taint is available. */
      if (!this->symbolicEngine->isEnabled()) {
        *this->backupSymbolicEngine = *this->symbolicEngine;
        this->backupAstGarbageCollector = this->astGarbageCollector;
      }
    }


    void IrBuilder::postIrInit(triton::arch::Instruction& inst) {
      std::set<triton::ast::AbstractNode*> uniqueNodes;
      std::vector<triton::engines::symbolic::SymbolicExpression*> newVector;

      /* Clear unused data */
      inst.memoryAccess.clear();
      inst.registerState.clear();

      /* Set the taint */
      inst.setTaint();

      // ----------------------------------------------------------------------

      /*
       * If the symbolic engine is disable we delete symbolic
       * expressions and AST nodes. Note that if the taint engine
       * is enable we must compute semanitcs to spread the taint.
       */
      if (!this->symbolicEngine->isEnabled()) {
        this->removeSymbolicExpressions(inst, uniqueNodes);
        *this->symbolicEngine = *this->backupSymbolicEngine;
      }

      // ----------------------------------------------------------------------

      /*
       * If the symbolic engine is defined to process symbolic
       * execution only on tainted instructions, we delete all
       * expressions untainted and their AST nodes.
       */
      if (this->modes.isModeEnabled(triton::modes::ONLY_ON_TAINTED) && !inst.isTainted()) {
        this->removeSymbolicExpressions(inst, uniqueNodes);
      }

      // ----------------------------------------------------------------------

      /*
       * If the symbolic engine is defined to process symbolic
       * execution only on symbolized expressions, we delete all
       * concrete expressions and their AST nodes.
       */
      if (this->symbolicEngine->isEnabled() && this->modes.isModeEnabled(triton::modes::ONLY_ON_SYMBOLIZED)) {
        /* Clean memory operands */
        this->collectUnsymbolizedNodes(uniqueNodes, inst.operands);

        /* Clean implicit and explicit semantics - MEM */
        this->collectUnsymbolizedNodes(uniqueNodes, inst.getLoadAccess());

        /* Clean implicit and explicit semantics - REG */
        this->collectUnsymbolizedNodes(uniqueNodes, inst.getReadRegisters());

        /* Clean implicit and explicit semantics - IMM */
        this->collectUnsymbolizedNodes(uniqueNodes, inst.getReadImmediates());

        /* Clean implicit and explicit semantics - MEM */
        this->collectUnsymbolizedNodes(uniqueNodes, inst.getStoreAccess(), true);

        /* Clean implicit and explicit semantics - REG */
        this->collectUnsymbolizedNodes(uniqueNodes, inst.getWrittenRegisters());

        /* Clean symbolic expressions */
        for (auto it = inst.symbolicExpressions.begin(); it != inst.symbolicExpressions.end(); it++) {
          if ((*it)->isSymbolized() == false) {
            this->astGarbageCollector.extractUniqueAstNodes(uniqueNodes, (*it)->getAst());
            this->symbolicEngine->removeSymbolicExpression((*it)->getId());
          }
          else
            newVector.push_back(*it);
        }
        inst.symbolicExpressions = newVector;
      }

      // ----------------------------------------------------------------------

      /*
       * If there is no symbolic expression, clean memory operands AST
       * and implicit/explicit semantics AST to avoid memory leak.
       */
      if (this->modes.isModeEnabled(triton::modes::ONLY_ON_TAINTED) && !inst.isTainted()) {
        /* Memory operands */
        this->collectUntaintedNodes(uniqueNodes, inst.operands);

        /* Implicit and explicit semantics - MEM */
        this->collectUntaintedNodes(uniqueNodes, inst.getLoadAccess());

        /* Implicit and explicit semantics - REG */
        this->collectUntaintedNodes(uniqueNodes, inst.getReadRegisters());

        /* Implicit and explicit semantics - IMM */
        this->collectUntaintedNodes(uniqueNodes, inst.getReadImmediates());

        /* Implicit and explicit semantics - MEM */
        this->collectUntaintedNodes(uniqueNodes, inst.getStoreAccess());

        /* Implicit and explicit semantics - REG */
        this->collectUntaintedNodes(uniqueNodes, inst.getWrittenRegisters());
      }

      // ----------------------------------------------------------------------

      /* Free collected nodes */
      this->astGarbageCollector.freeAstNodes(uniqueNodes);

      if (!this->symbolicEngine->isEnabled())
        this->astGarbageCollector = this->backupAstGarbageCollector;
    }


    void IrBuilder::removeSymbolicExpressions(triton::arch::Instruction& inst, std::set<triton::ast::AbstractNode*>& uniqueNodes) {
      for (auto it = inst.symbolicExpressions.begin(); it != inst.symbolicExpressions.end(); it++) {
        this->astGarbageCollector.extractUniqueAstNodes(uniqueNodes, (*it)->getAst());
        this->symbolicEngine->removeSymbolicExpression((*it)->getId());
      }
      inst.symbolicExpressions.clear();
    }


    template <class T>
    void IrBuilder::collectUntaintedNodes(std::set<triton::ast::AbstractNode*>& uniqueNodes, T& items) {
      for (auto it = items.cbegin(); it != items.cend(); it++)
        this->astGarbageCollector.extractUniqueAstNodes(uniqueNodes, std::get<1>(*it));
      items.clear();
    }


    void IrBuilder::collectUntaintedNodes(std::set<triton::ast::AbstractNode*>& uniqueNodes, std::vector<triton::arch::OperandWrapper>& operands) {
      for (auto it = operands.begin(); it != operands.end(); it++) {
        if (it->getType() == triton::arch::OP_MEM) {
          this->astGarbageCollector.extractUniqueAstNodes(uniqueNodes, it->getMemory().getLeaAst());
          it->getMemory().setLeaAst(nullptr);
        }
      }
    }


    template <class T>
    void IrBuilder::collectUnsymbolizedNodes(std::set<triton::ast::AbstractNode*>& uniqueNodes, T& items) {
      T newItems;

      for (auto it = items.cbegin(); it != items.cend(); it++) {
        if (std::get<1>(*it)->isSymbolized() == true)
          newItems.insert(*it);
      }

      items = newItems;
    }


    void IrBuilder::collectUnsymbolizedNodes(std::set<triton::ast::AbstractNode*>& uniqueNodes, std::set<std::pair<triton::arch::MemoryAccess, triton::ast::AbstractNode*>>& items, bool isStoreAccess) {
      std::set<std::pair<triton::arch::MemoryAccess, triton::ast::AbstractNode*>> newItems;

      for (auto it = items.cbegin(); it != items.cend(); it++) {
        if (std::get<1>(*it)->isSymbolized())
          newItems.insert(*it);
        if (isStoreAccess) {
          if (std::get<0>(*it).getLeaAst() != nullptr && std::get<0>(*it).getLeaAst()->isSymbolized())
            newItems.insert(*it);
        }
      }

      items = newItems;
    }


    void IrBuilder::collectUnsymbolizedNodes(std::set<triton::ast::AbstractNode*>& uniqueNodes, std::vector<triton::arch::OperandWrapper>& operands) {
      for (auto it = operands.begin(); it!= operands.end(); it++) {
        if (it->getType() == triton::arch::OP_MEM) {
          if (it->getMemory().getLeaAst()->isSymbolized() == false) {
            this->astGarbageCollector.extractUniqueAstNodes(uniqueNodes, it->getMemory().getLeaAst());
            it->getMemory().setLeaAst(nullptr);
          }
        }
      }
    }

  }; /* arch namespace */
}; /* triton namespace */
