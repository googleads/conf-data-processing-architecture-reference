terraform {
  backend "gcs" {
    bucket = "operator-terraform"
    prefix = "postsubmit-mp/jobservice.tfstate"
  }

  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 4.48"
    }
  }
}
