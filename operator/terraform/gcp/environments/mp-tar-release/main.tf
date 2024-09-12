terraform {
  backend "gcs" {
    bucket = "operator-terraform"
    prefix = "mp-release-tar/jobservice.tfstate"
  }

  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 4.48"
    }
  }
}
