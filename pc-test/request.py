import requests
import json
from machine import Pin

S0 = Pin(18, Pin.OUT)
# URL de l'API avec le paramètre where
url = "https://data.stib-mivb.brussels/api/explore/v2.1/catalog/datasets/vehicle-position-rt-production/records?select=*&where=lineid%3D71&limit=100"

# Clé API
headers = {
    'Authorization': 'Apikey ****'
}

# Liste des pointId à vérifier
target_point_ids = ["3558", "3559", "3525", "2351", "2397", "3510B", "3517", "3372"]

# Dictionnaire des pointId avec les noms des arrêts
point_names = {
    "3558": "Cim d'ix vers ULB 3558",
    "3559": "ULB vers Jeanne 3559",
    "3525": "Jeanne vers Buyl 3525",
    "2351": "Buyl(debroucker) vers Geo Bernier2351",
    "2397": "Buyl(delta) vers ULB 2397",
    "3510": "Geo Bernier vers Buyl 3510B",
    "3517": "Etang ixelles vers Geo Bernier 3517",
    "3372": "Flagey vers Etang 3372"
}

# Effectuer la requête avec la clé API dans les en-têtes
response = requests.get(url, headers=headers)

# Vérifier le contenu de la réponse
if response.status_code == 200:
    data = response.json()
    results = data.get("results", [])
    
    # Liste pour stocker les pointId présents dans la réponse
    found_point_ids = []
    all_point_ids = []  # Liste pour tous les pointId trouvés

    # Parcourir chaque élément de la réponse pour vérifier la présence des pointIds cibles
    for result in results:
        # Récupérer la liste des vehiclepositions et la parser
        vehicle_positions = json.loads(result.get("vehiclepositions", "[]"))  # Parser la chaîne JSON

        # Vérifier chaque pointId
        for position in vehicle_positions:
            point_id = position.get("pointId")
            distance_point = position.get("distanceFromPoint")
            all_point_ids.append(point_id)  # Ajouter tous les pointId rencontrés
            
            if point_id in target_point_ids and point_id not in found_point_ids:
                found_point_ids.append(point_id)
                
                # Afficher le pointId et son nom si disponible dans le dictionnaire
                point_name = point_names.get(point_id, f"Nom inconnu pour le pointId {point_id}")
                print(f"PointId trouvé: {point_id} - {point_name} - {distance_point}")
    
    # Si aucun pointId n'a été trouvé dans la liste cible
    if not found_point_ids:
        print("Aucun des pointIds cibles n'a été trouvé dans la réponse.")
        print("Liste de tous les pointIds trouvés :")
        print(all_point_ids)
else:
    print(f"Erreur {response.status_code}: {response.text}")
