import requests

url = "https://datn-mauve.vercel.app/api/telemetry"
data_4s = {
    "V": 15.600,
    "I": 0.017,
    "P": 0.265,
    "E": 0.000123,
    "T": 33.2,
    "SOC": 75,
    "SOH": 99,
    "CV": [3.900, 3.900, 3.900, 3.900],
    "CSOC": [75, 75, 75, 75],
    "CSOH": [99, 99, 99, 99],
    "CAI_Class": [0, 0, 0, 0],
    "AI_Class": 0,
    "AI_Score": 98.5,
    "AI_Time": 2,
    "Status": "Idle"
}

res = requests.post(url, json=data_4s, timeout=10)
print(f"Status Code: {res.status_code}")
print(f"Response Body: {res.text}")
