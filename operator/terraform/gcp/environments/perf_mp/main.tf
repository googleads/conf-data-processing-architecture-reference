terraform {
  backend "gcs" {
    bucket = "operator-terraform"
    prefix = "perf-mp/jobservice.tfstate"
  }

  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 4.48"
    }
  }
}
