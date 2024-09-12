environment = "postsubmit-mp"
project_id  = "admcloud-adtech1"
region      = "us-central1"
region_zone = "us-central1-c"

# Multi region location
# https://cloud.google.com/storage/docs/locations
operator_package_bucket_location = "US"

spanner_instance_config              = "regional-us-central1"
spanner_processing_units             = 100
spanner_database_deletion_protection = false

worker_image = "us-docker.pkg.dev/admcloud-scp/docker-repo-dev/worker_app_mp_gcp:postsubmit"
# Temporarily use the demo coordinator service until we are ready to integrate dev environments
allowed_operator_service_account = "postsubmit-a-opallowedusr@admcloud-coordinator1.iam.gserviceaccount.com,postsubmit-b-opallowedusr@admcloud-coordinator2.iam.gserviceaccount.com"
user_provided_worker_sa_email    = "postsubmit-mp-worker-sa@admcloud-adtech1.iam.gserviceaccount.com"
worker_logging_enabled           = true
worker_container_log_redirect    = "true"
worker_instance_force_replace    = true
worker_memory_monitoring_enabled = true
max_worker_instances             = 2
alarms_enabled                   = true
alarms_notification_email        = "fakeemail@google.com"

enable_job_completion_notifications = true

enable_job_completion_notifications_per_job           = true
job_completion_notifications_cloud_function_jar       = "/tmp/postsubmit_mp/jars/JobNotificationCloudFunction_deploy.jar"
job_completion_notifications_cloud_function_cpu_count = "1"
job_completion_notifications_cloud_function_memory_mb = "512"

worker_scale_in_jar  = "/tmp/postsubmit_mp/jars/WorkerScaleInCloudFunction_deploy.jar"
frontend_service_jar = "/tmp/postsubmit_mp/jars/FrontendServiceHttpCloudFunction_deploy.jar"
