CXX ?= c++
MPICXX ?= mpicxx
MPIEXEC ?= mpiexec
MPI_BUILD ?= build
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
PYTHON ?= python3
NUM = projects/numerical-methods
BELTS = projects/cpp-belts

.PHONY: all check cpp-check mpi-check mpi-sanitize constants-check belts-check belts-sanitize course-check course-sanitize python-check clean
all: build/inverse
build:
	mkdir -p build
build/inverse: $(NUM)/src/main.cpp $(NUM)/include/matrix.hpp $(NUM)/include/matrix_input.hpp $(NUM)/include/solver.hpp | build
	$(CXX) $(CXXFLAGS) -I$(NUM)/include $< -o $@
build/test-solver: $(NUM)/tests/test_solver.cpp $(NUM)/include/matrix.hpp $(NUM)/include/solver.hpp | build
	$(CXX) $(CXXFLAGS) -I$(NUM)/include $< -o $@
build/benchmark: $(NUM)/src/benchmark.cpp $(NUM)/include/matrix.hpp $(NUM)/include/solver.hpp | build
	$(CXX) $(CXXFLAGS) -I$(NUM)/include $< -o $@
build/compare-local: $(NUM)/comparison/local.cpp $(NUM)/comparison/common.hpp $(NUM)/include/matrix.hpp $(NUM)/include/solver.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
$(MPI_BUILD)/compare-mpi: $(NUM)/comparison/mpi.cpp $(NUM)/comparison/common.hpp $(NUM)/mpi/solver.cpp $(NUM)/mpi/solver.hpp $(NUM)/include/matrix.hpp
	mkdir -p "$(MPI_BUILD)"
	$(MPICXX) $(CXXFLAGS) $(NUM)/comparison/mpi.cpp $(NUM)/mpi/solver.cpp -o $@
cpp-check: build/inverse build/test-solver
	$(PYTHON) scripts/run_cpp_tests.py
$(MPI_BUILD)/inverse-mpi: $(NUM)/mpi/main.cpp $(NUM)/mpi/solver.cpp $(NUM)/mpi/solver.hpp $(NUM)/include/matrix.hpp $(NUM)/include/matrix_input.hpp
	mkdir -p "$(MPI_BUILD)"
	$(MPICXX) $(CXXFLAGS) $(NUM)/mpi/main.cpp $(NUM)/mpi/solver.cpp -o $@
$(MPI_BUILD)/test-mpi: $(NUM)/mpi/tests.cpp $(NUM)/mpi/solver.cpp $(NUM)/mpi/solver.hpp $(NUM)/include/matrix.hpp $(NUM)/include/solver.hpp
	mkdir -p "$(MPI_BUILD)"
	$(MPICXX) $(CXXFLAGS) $(NUM)/mpi/tests.cpp $(NUM)/mpi/solver.cpp -o $@
$(MPI_BUILD)/benchmark-mpi: $(NUM)/mpi/benchmark.cpp $(NUM)/mpi/solver.cpp $(NUM)/mpi/solver.hpp $(NUM)/include/matrix.hpp
	mkdir -p "$(MPI_BUILD)"
	$(MPICXX) $(CXXFLAGS) $(NUM)/mpi/benchmark.cpp $(NUM)/mpi/solver.cpp -o $@
mpi-check: $(MPI_BUILD)/inverse-mpi $(MPI_BUILD)/test-mpi
	MPI_BUILD="$(MPI_BUILD)" MPIEXEC="$(MPIEXEC)" $(PYTHON) scripts/check_mpi.py
mpi-sanitize:
	mkdir -p "$(MPI_BUILD)"
	$(MPICXX) -std=c++17 -O1 -Wall -Wextra -Wpedantic -pthread -fsanitize=undefined -fno-sanitize-recover=all $(NUM)/mpi/tests.cpp $(NUM)/mpi/solver.cpp -o $(MPI_BUILD)/test-mpi-ubsan
	MPI_BUILD="$(MPI_BUILD)" MPIEXEC="$(MPIEXEC)" $(PYTHON) scripts/check_mpi.py --test-binary $(MPI_BUILD)/test-mpi-ubsan --cpp-only
build/test-constants: projects/cpp-belts/constants/main.cpp projects/cpp-belts/constants/other.cpp projects/cpp-belts/constants/constants.hpp | build
	$(CXX) $(CXXFLAGS) projects/cpp-belts/constants/main.cpp projects/cpp-belts/constants/other.cpp -o $@
constants-check: build/test-constants
	./build/test-constants
build/search: $(BELTS)/search/main.cpp $(BELTS)/search/search.cpp $(BELTS)/search/search.hpp | build
	$(CXX) $(CXXFLAGS) $(BELTS)/search/main.cpp $(BELTS)/search/search.cpp -o $@
build/domains: $(BELTS)/domains/main.cpp $(BELTS)/domains/domains.hpp | build
	$(CXX) $(CXXFLAGS) $(BELTS)/domains/main.cpp -o $@
build/test-belts: $(BELTS)/tests/check.cpp $(BELTS)/tests/reference.hpp $(BELTS)/search/search.cpp $(BELTS)/search/search.hpp $(BELTS)/domains/domains.hpp | build
	$(CXX) $(CXXFLAGS) $(BELTS)/tests/check.cpp $(BELTS)/search/search.cpp -o $@
build/benchmark-belts: $(BELTS)/tests/benchmark.cpp $(BELTS)/tests/reference.hpp $(BELTS)/search/search.cpp $(BELTS)/search/search.hpp $(BELTS)/domains/domains.hpp | build
	$(CXX) $(CXXFLAGS) $(BELTS)/tests/benchmark.cpp $(BELTS)/search/search.cpp -o $@
belts-check: build/search build/domains build/test-belts
	$(PYTHON) scripts/check_belts.py
belts-sanitize: | build
	$(CXX) -std=c++17 -O1 -Wall -Wextra -Wpedantic -pthread -fsanitize=undefined -fno-sanitize-recover=all $(BELTS)/tests/check.cpp $(BELTS)/search/search.cpp -o build/test-belts-ubsan
	$(PYTHON) -c 'import subprocess; subprocess.run(["./build/test-belts-ubsan"], check=True, timeout=60)'
python-check:
	$(PYTHON) -m unittest discover -s tests -v
course-check:
	$(PYTHON) $(BELTS)/course/white/check.py
	$(PYTHON) $(BELTS)/course/yellow/check.py
	$(PYTHON) $(BELTS)/course/red/check.py
	$(PYTHON) $(BELTS)/course/brown/check.py
	$(PYTHON) $(BELTS)/course/black/check.py
	$(PYTHON) $(BELTS)/course/black/check_mython.py
	$(PYTHON) scripts/check_course_coverage.py
course-sanitize:
	$(PYTHON) $(BELTS)/course/white/check.py --sanitize
	$(PYTHON) $(BELTS)/course/yellow/check.py --sanitize
	$(PYTHON) $(BELTS)/course/red/check.py --sanitize
	$(PYTHON) $(BELTS)/course/brown/check.py --sanitize
	$(PYTHON) $(BELTS)/course/black/check.py --sanitize
	$(PYTHON) $(BELTS)/course/black/check_mython.py --sanitize
check: cpp-check constants-check belts-check course-check python-check
	$(PYTHON) scripts/check_links.py
	$(PYTHON) scripts/check_originals.py
	$(PYTHON) scripts/check_recorded_source.py
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'
