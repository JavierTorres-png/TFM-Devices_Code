import requests
import os

azure_ml_url = os.environ['AZURE_ML_URL']

azure_ml_headers = {
	'Content-Type': 'application/json',
	'Accept': 'application/json',
	'Authorization':('Bearer ' + os.environ['AZURE_ML_TOKEN'])}

def send_message_to_azure_ml(message):
	return requests.post(azure_ml_url, headers=azure_ml_headers, json=message)

def function_for_dev_ops():
	return "wki"
