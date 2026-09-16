from importlib import resources
import json
import os
import fitsolver

def main() -> None:
        here = os.path.dirname(os.path.abspath(__file__))       
        try:
                with open(os.path.join(here, input("Enter order JSON's relative path, or leave blank for default order file: "))) as oF:
                        items = json.load(oF)
        except FileNotFoundError:
                print("File not found. Using default items.json file.\n")
                items = json.load(resources.open_text("fitsolver","items.json"))
        try:
                with open(os.path.join(here, input("Enter box JSON's relative path, or leave blank for default box file: "))) as bF:
                        boxes = json.load(bF)
        except FileNotFoundError:
                print("File not found. Using default boxes.json file.\n")
                boxes = json.load(resources.open_text("fitsolver","boxes.json"))

        request = {"items": items, "boxes": boxes}
        response_json = fitsolver.solve(json.dumps(request))
        response = json.loads(response_json)

        print(json.dumps(response, indent=2))
        print()
        print(f"success: {response['success']}")
        print(f"boxes used: {len(response['boxes'])}")
        print(f"items placed: {len(response['placements'])}")
        print(f"items unplaced: {response['unplaced_item_codes']}")

        out_path = os.path.join(here, "result.json")
        with open(out_path, "w") as f:
                json.dump(response, f, indent=2)
        print(f"\nWrote full result to {out_path}")
        input("Press [Enter] to exit.")

if __name__ == "__main__":
        main()
