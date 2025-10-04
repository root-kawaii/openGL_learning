import json

# Read the JSON file
with open('levels/two.json', 'r') as f:
    data = json.load(f)

# Add "entity": "88" to all objects with path "assets/cube.obj"
for obj in data['objects']:
    if obj.get('path') == 'assets/cube.obj':
        obj['entity'] = "88"

# Write the modified JSON back to a file
with open('output.json', 'w') as f:
    json.dump(data, f, indent=4)

print(f"Processed {len(data['objects'])} objects")
print(f"Added 'entity' field to all cube objects")
