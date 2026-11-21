import importlib.util
from pathlib import Path

recipe_path = Path(__file__).resolve().parent.parent / "conan" / "conanfile.py"
spec = importlib.util.spec_from_file_location("anura_conan", recipe_path)
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)


class Anura(recipe.Anura):
    generators = ["CMakeDeps"]
