import random

def decode_payload(payload):
    # Assurez-vous que le payload est une liste de 10 bytes
    if len(payload) != 10:
        raise ValueError("Le payload doit contenir exactement 10 bytes")

    # Décoder la latitude
    lat = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3]
    latitude = lat / 1e5  # Convertir en nombre à virgule flottante

    # Décoder la longitude
    lon = (payload[4] << 24) | (payload[5] << 16) | (payload[6] << 8) | payload[7]
    longitude = lon / 1e5  # Convertir en nombre à virgule flottante

    return latitude, longitude

# Nouvelle latitude et longitude
new_latitude = 50.865913
new_longitude = 4.339343

#new_latitude = 50.848395
#new_longitude = 4.454977

# Convertir en entiers
lat_int = int(new_latitude * 1e5)
lon_int = int(new_longitude * 1e5)

# Générer un chiffre aléatoire entre 50 et 100 pour le 9ème byte
random_byte = random.randint(50, 100)

# Convertir en bytes
payload = [
    (lat_int >> 24) & 0xFF,
    (lat_int >> 16) & 0xFF,
    (lat_int >> 8) & 0xFF,
    lat_int & 0xFF,
    (lon_int >> 24) & 0xFF,
    (lon_int >> 16) & 0xFF,
    (lon_int >> 8) & 0xFF,
    lon_int & 0xFF,
    0x00, random_byte  # Les deux derniers bytes peuvent être utilisés pour d'autres informations
]

# Imprimer le payload en hexadécimal
hex_payload = [f'{byte:02X}' for byte in payload]
print(f'Payload en hexadécimal: {hex_payload}')
latitude, longitude = decode_payload(payload)
print(f"Latitude: {latitude}")
print(f"Longitude: {longitude}")