import requests
import os
from datetime import datetime, timezone
from email.utils import parsedate_to_datetime

azure_ml_url = os.environ['AZURE_ML_URL']
azure_ml_headers = {
	'Content-Type': 'application/json',
	'Accept': 'application/json',
	'Authorization':('Bearer ' + os.environ['AZURE_ML_TOKEN'])}
azure_container_url = os.environ['AZURE_CONTAINER_URL']
esp32_code_file = os.path.expanduser("~/Documentos/esp32_code.bin")

def send_message_to_azure_ml(message):
	return requests.post(azure_ml_url, headers=azure_ml_headers, json=message)


# This thread checks for updates of the code in the blob storage
def check_for_update():
	success = False

	local_timestamp = os.path.getmtime(esp32_code_file)
	local_timestamp = datetime.fromtimestamp(local_timestamp, timezone.utc)

	head = requests.head(azure_container_url)
	if (head.status_code != 200):
		print(f"Error: {head.status.code}")
		return success

	remote_timestamp = head.headers.get('Last-Modified')
	if not remote_timestamp:
		print("Couldn't obtain last modification from remote file")
		return success

	# Ensure timestamp has the same format
	remote_timestamp = parsedate_to_datetime(remote_timestamp)
	# If time difference between local file and remote file is greater than 10 seconds, then they are not the same file
	if (abs(local_timestamp - remote_timestamp).total_seconds() > 10 and local_timestamp < remote_timestamp):
		print("New file detected in remote: " + str(remote_timestamp))
		response = requests.get(azure_container_url, stream=True)
		if (response.status_code == 200):
			#Update local file
			with open(esp32_code_file, "wb") as f:
				for chunk in response.iter_content(chunk_size=8192):
					if chunk:
						f.write(chunk)
			success=True
	else:
		print("File is already updated")
	return success
