provider "aws" {
  profile = "default"
  region  = var.aws_region
  default_tags {
    tags = {
      "mrpid:pid-id" = var.pid_id
    }
  }
}

terraform {
  backend "s3" {}
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}
