# Fantastic FitSolver Package
The distributable Python package for the FitSolver C++ program, which, given two JSON files of available boxes and items to fit in boxes, returns a JSON file with all items packed into the fewest and smallest boxes possible.

Source code, tests and more in-depth documentation can be found at: https://github.com/FantasticDivision/Solver/

# Example program

The below code can be used to test if the package has installed correctly. To test, save as a .py file and run; it should prompt two inputs (you can just press ENTER to use the default item/box files) and return a sufficiently packed JSON.

Example box and item JSON files come with the fitsolver package, and can be called in a script using ``` resources.open_text("fitsolver","[items || boxes].json") ``` (need to put ``` from importlib import resources ``` in imports). 

```
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

```