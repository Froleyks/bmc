#include "aiger.hpp"
#include "cadical.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>

CaDiCaL::Solver solver;

auto arguments(int argc, char *argv[]) {
  auto usage = [&argv]() {
    std::cerr
        << "Usage: " << argv[0]
        << " [ <max_max_bound> ] <aiger> [<witness>] [--no-xor] [--no-ite] "
           "[--cadical_option]\n";
    exit(1);
  };
  char *err, *aiger{}, *witness{};
  unsigned long max_bound;
  bool noxor{}, noite{};
  for (int i = 1; i < argc; ++i) {
    if (i == 1) {
      if (!strcmp(argv[i], "--version"))
        std::cout << VERSION << "-" << CaDiCaL::Solver::version() << '\n',
            exit(0);
      max_bound = strtoul(argv[i], &err, 10);
      if (*err)
        max_bound = 1000;
      else
        continue;
    }
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      if (!strcmp(argv[i], "--xor"))
        noxor = true;
      else if (!strcmp(argv[i], "--ite"))
        noite = true;
      else if (!CaDiCaL::Solver::is_valid_long_option(argv[i]))
        std::cerr << "c cannot parse option: " << argv[i] << std::endl, exit(1);
      solver.set_long_option(argv[i]);
    } else if (!aiger)
      aiger = argv[i];
    else if (!witness)
      witness = argv[i];
    else
      usage();
  }
  if (!aiger) usage();
  return std::tuple{max_bound, aiger, witness, noxor, noite};
}

unsigned bound{};
std::vector<std::vector<int>> var;
bool val(unsigned b, unsigned lit) { return solver.val(var.at(b).at(lit)) > 0; }
void assume(int lit) { solver.assume(var.at(bound).at(lit)); }
void next(int lit) { solver.add(var.at(bound + 1).at(lit)); }
template <typename... Args> void clause(Args... lits) {
  (solver.add(var.at(bound).at(lits)), ...);
  solver.add(0);
}

void extend(unsigned num_literals) {
  var.emplace_back();
  var.back().reserve(num_literals);
  int v{solver.vars()};
  for (unsigned i = 0; i < num_literals; i += 2) {
    var.back().push_back(++v);
    var.back().push_back(-v);
  }
  solver.reserve(v);
  clause(1); // truth constant
}

void gate_extraction(AIG &aig, bool noxor, bool noite) {
  aiger *aiger = aig.aig;
  for (int i = 0; i < aiger->num_ands; i++) {
    unsigned not_lhs, not_rhs0, not_rhs1;
    unsigned cond_lit, then_lit, else_lit;
    unsigned not_cond, not_then, not_else;
    unsigned lhs, rhs0, rhs1;
    aiger_and *and_g;
    int xor_g, ite;
    and_g = aiger->ands + i;
    lhs = and_g->lhs;
    rhs0 = and_g->rhs0;
    rhs1 = and_g->rhs1;
    xor_g = !noxor && is_xor(aiger, lhs, &rhs0, &rhs1);
    ite = !xor_g && !noite && is_ite(aiger, lhs, &cond_lit, &then_lit, &else_lit);
    not_lhs = aiger_not(lhs);
    not_rhs0 = aiger_not(rhs0);
    not_rhs1 = aiger_not(rhs1);
    not_cond = aiger_not(cond_lit);
    not_then = aiger_not(then_lit);
    not_else = aiger_not(else_lit);
    if (xor_g) {
      clause(not_lhs, rhs0, rhs1);
      clause(not_lhs, not_rhs0, not_rhs1);
    } else if (ite) {
      clause(not_lhs, not_cond, then_lit);
      clause(not_lhs, cond_lit, else_lit);
    } else {
      clause(not_lhs, rhs1);
      clause(not_lhs, rhs0);
    }
    if (xor_g) {
      clause(lhs, rhs0, not_rhs1);
      clause(lhs, not_rhs0, rhs1);
    } else if (ite) {
      clause(lhs, not_cond, not_then);
      clause(lhs, cond_lit, not_else);
    } else
      clause(lhs, not_rhs1, not_rhs0);
  }
}

int main(int argc, char *argv[]) {
  const auto start = std::chrono::steady_clock::now();
  auto time = [&start]() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(steady_clock::now() - start).count();
  };
  auto [max_bound, aiger_path, witness_path, noxor, noite] =
      arguments(argc, argv);
  AIG aiger{aiger_path};

  extend(aiger.size());
  for (auto [l, r] : aiger.latches() | resets) {
    clause(1 ^ l, r);
    clause(1 ^ r, l);
  }
  bool sat = false;
  for (; bound < max_bound && !sat; ++bound) {
    extend(aiger.size());
    if (!noxor || !noite) {
      gate_extraction(aiger, noxor, noite);
    } else {
      for (auto [a, x, y] : aiger.ands()) {
        clause(1 ^ x, 1 ^ y, a);
        clause(1 ^ a, x);
        clause(1 ^ a, y);
      }
    }
    for (auto c : aiger.constraints() | lits)
      clause(c);
    for (auto [l, n] : aiger.latches() | nexts) {
      next(1 ^ l), clause(n);
      next(l), clause(1 ^ n);
    }
    assume(aiger.output());
    sat = solver.solve() == 10;
    std::cout << bound << " " << time() << std::endl;
  }

  if (bound == max_bound) {
    std::cout << "c reached max bound " << bound << std::endl;
    return 124;
  }
  std::cout << "c SAT\n";
  if (!witness_path) return 0;
  std::ofstream witness(witness_path);
  witness << "1\nb0\n";
  for (auto l : aiger.latches() | lits)
    witness << val(0, l);
  witness << "\n";
  for (int i = 0; i <= bound; ++i) {
    for (auto l : aiger.inputs() | lits)
      witness << val(i, l);
    witness << "\n";
  }
  witness << ".\n";
  std::cout << "c Witness written to " << witness_path << std::endl;
}
