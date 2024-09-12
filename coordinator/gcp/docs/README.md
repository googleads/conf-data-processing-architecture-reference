## Coordinator setup

Welcome to the README for setting up AWS coordinator Service!

Please follow the instructions below to set up the multi-party coordinator services in AWS.

## Build pre-requisites
1. Install Terraform (v1.2.3)

## Steps to deploy primary coordinator
1. Create a new environment directory in the environments directory and copy the contents of [terraform/gcp/environments_mp_primary/demo](/coordinator/terraform/gcp/environments_mp_primary/demo) to the new environment directory
2. Configure GCP credentials using gcloud
3. (one time setup) Follow [README](/coordinator/terraform/gcp/environments_mp_primary/demo/allowedoperatorgroup/README.md) in your new environment directory to set up allowed operator group
4. Follow [README](/coordinator/terraform/gcp/environments_mp_primary/demo/mpkhs_primary/README.md) in your new environment directory to set up key hosting service resources
5. Follow [README](/coordinator/terraform/gcp/environments_mp_primary/demo/operator_wipp/README.md) in your new environment directory to set up operator access to the private key service

## Steps to deploy secondary coordinator
1. Create a new environment directory in the environments directory and copy the contents of [terraform/gcp/environments_mp_secondary/demo](/coordinator/terraform/gcp/environments_mp_secondary/demo) to the new environment directory
2. Configure GCP credentials using gcloud
3. (one time setup) Follow [README](/coordinator/terraform/gcp/environments_mp_secondary/demo/allowedoperatorgroup/README.md) in your new environment directory to set up allowed operator group
4. Follow [README](/coordinator/terraform/gcp/environments_mp_secondary/demo/mpkhs_secondary/README.md) in your new environment directory to set up key hosting service resources
5. Follow [README](/coordinator/terraform/gcp/environments_mp_secondary/demo/operator_wipp/README.md) in your new environment directory to set up operator access to the private key service
