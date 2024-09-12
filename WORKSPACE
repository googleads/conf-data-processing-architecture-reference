load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

################################################################################
# Rules JVM External: Begin
################################################################################
load("//build_defs/java:rules_jvm_external.bzl", "rules_jvm_external")

rules_jvm_external()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()
################################################################################
# Rules JVM External: End
################################################################################
################################################################################
# Download all http_archives and git_repositories: Begin
################################################################################

# Declare explicit protobuf version and hash, to override any implicit dependencies.
# Please update both while upgrading to new versions.
PROTOBUF_CORE_VERSION_FOR_CC = "24.4"

PROTOBUF_SHA_256_FOR_CC = "616bb3536ac1fff3fb1a141450fa28b875e985712170ea7f1bfe5e5fc41e2cd8"

#############################
# CC SDK Dependencies Rules #
#############################

load("//build_defs/cc:sdk.bzl", "sdk_dependencies")

sdk_dependencies(PROTOBUF_CORE_VERSION_FOR_CC, PROTOBUF_SHA_256_FOR_CC)

#############
# CPP Rules #
#############
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies", "rules_cc_toolchains")

rules_cc_dependencies()

rules_cc_toolchains()

###########################
# CC Dependencies #
###########################

# Load indirect dependencies due to
#     https://github.com/bazelbuild/bazel/issues/1943
load("@com_github_googleapis_google_cloud_cpp//bazel:google_cloud_cpp_deps.bzl", "google_cloud_cpp_deps")

google_cloud_cpp_deps()

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    grpc = True,
    java = True,
)

##########
# GRPC C #
##########
# These dependencies from @com_github_grpc_grpc need to be loaded after the
# google cloud deps so that the grpc version can be set by the google cloud deps
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

###############
# Proto rules #
###############
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# Boost
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")

boost_deps()

# Foreign CC
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

#################################
# SCP Shared Dependencies Rules #
#################################

# This bazel file contains all the dependencies in SCP, except the dependencies
# only used in SDK. Eventually, each project will have its own bazel file for
# its dependencies, and this file will be removed.
load("//build_defs:scp_dependencies.bzl", "scp_dependencies")

scp_dependencies(PROTOBUF_CORE_VERSION_FOR_CC, PROTOBUF_SHA_256_FOR_CC)

######### To gegerate Java interface for SDK #########
load("@com_google_api_gax_java//:repository_rules.bzl", "com_google_api_gax_java_properties")

com_google_api_gax_java_properties(
    name = "com_google_api_gax_java_properties",
    file = "@com_google_api_gax_java//:dependencies.properties",
)

load("@com_google_api_gax_java//:repositories.bzl", "com_google_api_gax_java_repositories")

com_google_api_gax_java_repositories()

load("@io_grpc_grpc_java//:repositories.bzl", "grpc_java_repositories")

grpc_java_repositories()
##########################################################

################################################################################
# Download all http_archives and git_repositories: End
################################################################################

################################################################################
# Download Maven Dependencies: Begin
################################################################################
load("//build_defs/java:maven_dependencies.bzl", "maven_dependencies")

maven_dependencies()

################################################################################
# Download Maven Dependencies: End
################################################################################

################################################################################
# Download Indirect Dependencies: Begin
################################################################################
# Note: The order of statements in this section is extremely fragile
load("@rules_java//java:repositories.bzl", "rules_java_dependencies", "rules_java_toolchains")

rules_java_dependencies()

rules_java_toolchains()

# Load dependencies for the base workspace.
load("@com_google_differential_privacy//:differential_privacy_deps.bzl", "differential_privacy_deps")

differential_privacy_deps()

#############
# PKG Rules #
#############
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

################################################################################
# Download Indirect Dependencies: End
################################################################################

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")

############
# Go rules #
############
# Need to be after grpc_extra_deps to share go_register_toolchains.
load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")

go_rules_dependencies()

gazelle_dependencies()

###################
# Container rules #
###################
load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")

container_deps()

###########################
# Binary Dev Dependencies #
###########################
load("@com_github_google_rpmpack//:deps.bzl", "rpmpack_dependencies")
load("@io_bazel_rules_docker//container:container.bzl", "container_pull")

rpmpack_dependencies()

################################################################################
# Download Containers: Begin
################################################################################

# Distroless image for running Java.
container_pull(
    name = "java_base",
    # Using SHA-256 for reproducibility. The tag is latest-amd64. Latest as of 2023-06-12.
    digest = "sha256:1e4181aaff242e2b305bb4abbe811eb122d68ffd7fd87c25c19468a1bc387ce6",
    registry = "gcr.io",
    repository = "distroless/java17-debian11",
)

# Distroless debug image for running Java. Need to use debug image to install more dependencies for CC.
container_pull(
    name = "java_debug_runtime",
    # Using SHA-256 for reproducibility.
    digest = "sha256:f311c37af17ac6e96c44f5990aa2bb5070da32d5e4abc11b2124750c1062c308",
    registry = "gcr.io",
    repository = "distroless/java17-debian11",
    tag = "debug-nonroot-amd64",
)

# Distroless image for running C++.
container_pull(
    name = "cc_base",
    registry = "gcr.io",
    repository = "distroless/cc",
    # Using SHA-256 for reproducibility.
    # TODO: use digest instead of tag, currently it's not working.
    tag = "latest",
)

# Distroless image for running statically linked binaries.
container_pull(
    name = "static_base",
    registry = "gcr.io",
    repository = "distroless/static",
    # Using SHA-256 for reproducibility.
    # TODO: use digest instead of tag, currently it's not working.
    tag = "latest",
)

# Needed for reproducibly building AL2 binaries (e.g. //cc/proxy)
container_pull(
    name = "amazonlinux_2",
    # Latest as of 2023-06-12.
    digest = "sha256:cd3d9deffbb15db51382022a67ad717c02e0573c45c312713c046e4c2ac07771",
    registry = "index.docker.io",
    repository = "amazonlinux",
    tag = "2.0.20230530.0",
)

# Needed for reproducibly building AL2023 binaries (e.g. //cc/proxy)
container_pull(
    name = "amazonlinux_2023",
    # Latest as of Aug 29, 2023.
    digest = "sha256:adde60852d11d75196f747c54ae32509d97827369499839b607a6c34c23b2165",
    registry = "index.docker.io",
    repository = "amazonlinux",
    tag = "2023.1.20230825.0",
)

# Needed for cc/pbs/deploy/pbs_server/build_defs
container_pull(
    name = "debian_11",
    digest = "sha256:640e07a7971e0c13eb14214421cf3d75407e0965b84430e08ec90c336537a2cf",
    registry = "index.docker.io",
    repository = "amd64/debian",
    tag = "11",
)

########################################################################
# Roma dependencies
load("//build_defs/cc:roma.bzl", "roma_dependencies")

roma_dependencies()

load(
    "@com_google_sandboxed_api//sandboxed_api/bazel:llvm_config.bzl",
    "llvm_disable_optional_support_deps",
)

# Must call install_v8_python_deps to make sure the requirements are installed.
load("@v8_python_deps//:requirements.bzl", install_v8_python_deps = "install_deps")

install_v8_python_deps()

# Must be right after roma_dependencies
load(
    "@com_google_sandboxed_api//sandboxed_api/bazel:sapi_deps.bzl",
    "sapi_deps",
)

llvm_disable_optional_support_deps()

sapi_deps()
########################################################################

# Needed for cc reproducible builds
load("//cc/tools/build:build_container_params.bzl", "CC_BUILD_CONTAINER_REGISTRY", "CC_BUILD_CONTAINER_REPOSITORY", "CC_BUILD_CONTAINER_TAG")

container_pull(
    name = "prebuilt_cc_build_container_image_pull",
    registry = CC_BUILD_CONTAINER_REGISTRY,
    repository = CC_BUILD_CONTAINER_REPOSITORY,
    tag = CC_BUILD_CONTAINER_TAG,
)

##########################
## Closure dependencies ##
##########################
load("//build_defs/shared:bazel_rules_closure.bzl", "bazel_rules_closure")

bazel_rules_closure()

load(
    "@io_bazel_rules_closure//closure:repositories.bzl",
    "rules_closure_dependencies",
    "rules_closure_toolchains",
)

rules_closure_dependencies()

rules_closure_toolchains()

################################################################################
# Download Containers: End
################################################################################
