ADM Cloud Team's CMRT (Common Multi-cloud RunTime) Repo.

# Common Multi-cloud RunTime

CMRT (Common Multi-cloud RunTime) provides facilities to deploy and operate applications in a secure execution environment at scale. This includes managing cryptographic keys, keeping track of the privacy budget, accessing storage, orchestrating a policy-based horizontal autoscaling, and providing cross-cloud interface to access cloud services.

# Building

This repo depends on Bazel 4.2.2 with JDK 11 and a C++17 compiler (we use GCC9 and Clang13 to build and verify this project).  The following environment variables should be set in your local environment (the exact location will depend on your environment):

```
JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
# For GCC 9
CXX=/usr/bin/g++-9
CC=/usr/bin/gcc-9
# For Clang 13
CXX=/usr/bin/clang++-13
CC=/usr/bin/clang-13
```

The `.bazelrc` file specifies the minimum JDK and GCC version.

To build the repo, run `bazel build ...`.

# Contribution

Please see CONTRIBUTING.md for details.
