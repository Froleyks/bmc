#pragma once

#include <cassert>
#include <iostream>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

// Wrapper around the aiger library.
extern "C" {
#include "aiger.h"
}

// Read only circuit for model and witness.
struct AIG {
  aiger *aig;
  AIG(const char *path) : aig(aiger_init()) {
    const char *err = aiger_open_and_read_from_file(aig, path);
    if (err) {
      std::cerr << "Aiger: parse error reading " << path << ": " << err << "\n";
      exit(1);
    }
    if (aig->num_justice + aig->num_fairness)
      std::cout << "Aiger: WARNING justice and fairness are not supported "
                << path << "\n";
    if (aig->num_bad + aig->num_outputs > 1)
      std::cout << "Aiger: WARNING Multiple properties. Using "
                << (aig->num_bad ? "bad" : "output") << "0 of " << path << "\n";
  }
  ~AIG() { aiger_reset(aig); }

  // I don't want to un-const the input circuits, so I'm going to use a
  // const_cast
  bool is_input(unsigned l) const { return aiger_is_input(aig, l); }
  bool is_latch(unsigned l) const { return aiger_is_latch(aig, l); }

  unsigned reset(unsigned l) const {
    assert(is_latch(l));
    return aiger_is_latch(aig, l)->reset;
  }
  unsigned next(unsigned l) const {
    assert(is_latch(l));
    return aiger_is_latch(aig, l)->next;
  }
  unsigned size() const { return (aig->maxvar + 1) * 2; }

  std::span<aiger_symbol> inputs() const {
    return {aig->inputs, aig->num_inputs};
  }
  std::span<aiger_symbol> latches() const {
    return {aig->latches, aig->num_latches};
  }
  auto ands() const {
    return std::span{aig->ands, aig->num_ands} |
           std::views::transform([](const aiger_and &gate) {
             return std::tuple{gate.lhs, gate.rhs0, gate.rhs1};
           });
  }
  std::span<aiger_symbol> constraints() const {
    return {aig->constraints, aig->num_constraints};
  }
  unsigned output() const {
    if (aig->num_bad)
      return aig->bad[0].lit;
    else if (aig->num_outputs)
      return aig->outputs[0].lit;
    else
      return aiger_false;
  }
};

auto lits = std::views::transform([](const auto &l) { return l.lit; });
auto nexts = std::views::transform(
    [](const auto &l) { return std::pair{l.lit, l.next}; });
auto resets = std::views::transform(
    [](const auto &l) { return std::pair{l.lit, l.reset}; });
auto initialized =
    std::views::filter([](const auto &l) { return l.reset != l.lit; });
auto uninitialized =
    std::views::filter([](const auto &l) { return l.reset == l.lit; });

int have_same_variable(unsigned a, unsigned b) {
  return aiger_strip(a) == aiger_strip(b);
}

int distinct_variables(unsigned a, unsigned b, unsigned c) {
  if (aiger_strip(a) == aiger_strip(b)) return 0;
  if (aiger_strip(a) == aiger_strip(c)) return 0;
  if (aiger_strip(b) == aiger_strip(c)) return 0;
  return 1;
}

int is_xor(aiger *aiger, unsigned lit, unsigned *rhs0ptr, unsigned *rhs1ptr) {
  aiger_and *and_g, *left, *right;
  unsigned left_rhs0, left_rhs1;
  unsigned right_rhs0, right_rhs1;
  unsigned not_right_rhs0, not_right_rhs1;
  and_g = aiger_is_and(aiger, lit);
  if (!and_g) return 0;
  if (!aiger_sign(and_g->rhs0)) return 0;
  if (!aiger_sign(and_g->rhs1)) return 0;
  left = aiger_is_and(aiger, and_g->rhs0);
  if (!left) return 0;
  right = aiger_is_and(aiger, and_g->rhs1);
  if (!right) return 0;
  left_rhs0 = left->rhs0, left_rhs1 = left->rhs1;
  right_rhs0 = right->rhs0, right_rhs1 = right->rhs1;
  not_right_rhs0 = aiger_not(right_rhs0);
  not_right_rhs1 = aiger_not(right_rhs1);
  //      (!l0 | !l1) & (!r0 | !r1)
  // (A): ( r0 |  r1) & (!r0 | !r1)
  // (B): ( r1 |  r0) & (!r0 | !r1)
  //        r0 ^  r1                 // used
  //        l0 ^  l1                 // not used
  if ((left_rhs0 == not_right_rhs0 && left_rhs1 == not_right_rhs1) || //(A)
      (left_rhs0 == not_right_rhs1 && left_rhs1 == not_right_rhs0)) { //(B)
    const unsigned rhs0 = left_rhs0, rhs1 = left_rhs1;
    if (have_same_variable(rhs0, rhs1)) return 0;
    if (rhs0ptr) *rhs0ptr = rhs0;
    if (rhs1ptr) *rhs1ptr = rhs1;
    return 1;
  }
  return 0;
}

int is_ite(aiger *aiger, unsigned lit, unsigned *cond_lit_ptr,
           unsigned *then_lit_ptr, unsigned *else_lit_ptr) {
  aiger_and *and_g, *left, *right;
  unsigned left_rhs0, left_rhs1;
  unsigned right_rhs0, right_rhs1;
  unsigned not_left_rhs0, not_left_rhs1;
  unsigned not_right_rhs0, not_right_rhs1;
  unsigned cond_lit, then_lit, else_lit;
  int res = 1;
  and_g = aiger_is_and(aiger, lit);
  if (!and_g) return 0;
  if (!aiger_sign(and_g->rhs0)) return 0;
  if (!aiger_sign(and_g->rhs1)) return 0;
  left = aiger_is_and(aiger, and_g->rhs0);
  if (!left) return 0;
  right = aiger_is_and(aiger, and_g->rhs1);
  if (!right) return 0;
  left_rhs0 = left->rhs0, left_rhs1 = left->rhs1;
  right_rhs0 = right->rhs0, right_rhs1 = right->rhs1;
  not_left_rhs0 = aiger_not(left_rhs0);
  not_left_rhs1 = aiger_not(left_rhs1);
  not_right_rhs0 = aiger_not(right_rhs0);
  not_right_rhs1 = aiger_not(right_rhs1);
  if (left_rhs0 == not_right_rhs0) {
    // (!l0 | !l1) & (!r0 | !r1)
    // (!l0 | !l1) & (l0 | !r1)
    // (l0 -> !l1) & (!l0 -> !r1)
    // l0 ? !l1 : !r1
    cond_lit = left_rhs0;
    then_lit = not_left_rhs1;
    else_lit = not_right_rhs1;
  } else if (left_rhs0 == not_right_rhs1) {
    // (!l0 | !l1) & (!r0 | !r1)
    // (!l0 | !l1) & (!r0 | l0)
    // (l0 -> !l1) & (!r0 <- !l0)
    // l0 ? !l1 : !r0
    cond_lit = left_rhs0;
    then_lit = not_left_rhs1;
    else_lit = not_right_rhs0;
  } else if (left_rhs1 == not_right_rhs0) {
    // (!l0 | !l1) & (!r0 | !r1)
    // (!l0 | !l1) & (l1 | !r1)
    // (!l0 <- l1) & (!l1 -> !r1)
    // l1 ? !l0 : !r1
    cond_lit = left_rhs1;
    then_lit = not_left_rhs0;
    else_lit = not_right_rhs1;
  } else if (left_rhs1 == not_right_rhs1) {
    // (!l0 | !l1) & (!r0 | !r1)
    // (!l0 | !l1) & (!r0 | l1)
    // (!l0 <- l1) & (!r0 <- !l1)
    // l1 ? !l0 : !r0
    cond_lit = left_rhs1;
    then_lit = not_left_rhs0;
    else_lit = not_right_rhs0;
  } else
    return 0;
  if (aiger_is_constant(cond_lit)) return 0;
  if (aiger_is_constant(then_lit)) return 0;
  if (aiger_is_constant(else_lit)) return 0;
  if (!distinct_variables(cond_lit, then_lit, else_lit)) return 0;
  if (cond_lit_ptr) *cond_lit_ptr = cond_lit;
  if (then_lit_ptr) *then_lit_ptr = then_lit;
  if (else_lit_ptr) *else_lit_ptr = else_lit;
  return 1;
}
