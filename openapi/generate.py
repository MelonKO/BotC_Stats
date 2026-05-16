# openapi/generate.py
"""Генерация Pydantic-моделей из OpenAPI схемы."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent.parent
INPUT = Path(__file__).parent / "dist" / "openapi.json"
# OUTPUT = ROOT / "core" / "app" / "generated" / "models.py"
OUTPUT = Path(__file__).parent / "python_gen" / "models.py"

OUTPUT.parent.mkdir(parents=True, exist_ok=True)

result = subprocess.run(
    [
        sys.executable, "-m", "datamodel_code_generator",
        "--input", str(INPUT),
        "--input-file-type", "openapi",
        "--output", str(OUTPUT),
        "--output-model-type", "pydantic_v2.BaseModel",
        "--use-annotated",
        "--field-constraints",
    ],
    check=False,
)

if result.returncode != 0:
    print("ERROR: datamodel-codegen failed")
    sys.exit(result.returncode)

print(f"Generated: {OUTPUT}")
