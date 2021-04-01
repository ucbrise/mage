MAGE: Memory-Aware Garbling Engine
==================================
MAGE is an execution engine for secure computation protocols, such as secure multi-party computation (SMPC) and homomorphic encryption (HE). MAGE is designed to execute secure computation efficiently even when it does not fit in memory.

The implementation of MAGE in this repository accompanies our OSDI 2021 paper:

Sam Kumar, David E. Culler, and Raluca Ada Popa. Nearly Zero-Cost Virtual Memory for Secure Computation. To appear at OSDI 2021.

**WARNING: This implementation is a prototype designed for academic study and proof-of-concept use cases. It has not received code review and is *not* production-ready.**

Secure computation is inherently *oblivious*&mdash;that is, it contains no data-dependent memory accesses. The reason for this lies in maintaining security if memory accesses depended on the data, then an attacker could potentially analyze the memory access pattern and infer the contents of sensitive data. This is a problem because the point of using secure computation is to compute on sensitive data without revealing the contents of that data.

MAGE leverages the *oblivious* nature of secure computation to manage memory efficiently. Specifically, MAGE introduces a planning phase in which it analyzes in advance the computation it is going to perform. The result of MAGE's planning phase is a *memory program*, an execution plan for performing the computation. The memory program can be understood as (roughly) a pre-processed execution trace (with all functions inlined and all loops unrolled), including preplanned data transfers between memory and storage. At runtime, MAGE uses the memory program to transfer data between memory and storage very efficiently, in effect providing virtual memory at a very low cost.

How to Build and Use MAGE
-------------------------
Instructions to build MAGE, and a tutorial for using it, are available on the [MAGE wiki](https://github.com/ucbrise/mage/wiki).

To build documentation, run `doxygen` in the repository's root directory.

License
-------
The code in this repository is available under version 3 of the GNU General Public License (GPL).
