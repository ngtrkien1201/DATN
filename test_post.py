import requests
url = "https://datn-mauve.vercel.app/api/telemetry"
data = {"V":3.976,"I":0.017,"T":33.2,"SOC":75,"SOH":99}
res = requests.post(url, json=data)
print(res.status_code)
print(res.text)
