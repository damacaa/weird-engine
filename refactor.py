import os
import re

files_to_check = [
    "/run/media/damaca/data/projects/weird-engine/include/weird-engine/math/MathExpressions.h",
    "/run/media/damaca/data/projects/weird-engine/include/weird-engine/math/Primitives.h",
    "/run/media/damaca/data/projects/weird-engine/include/weird-engine/math/Primitives3D.h",
    "/run/media/damaca/data/projects/weird-engine/include/weird-engine/systems/ButtonSystem.h",
    "/run/media/damaca/data/projects/weird-engine/examples/sample-scenes/include/SceneLoadExample.h",
    "/run/media/damaca/data/projects/weird-engine/src/weird-physics/Simulation2D.cpp",
]

for filepath in files_to_check:
    if not os.path.exists(filepath):
        print(f"Not found: {filepath}")
        continue
        
    with open(filepath, "r") as f:
        content = f.read()

    # 1. Remove propagateValues declarations and definitions in headers
    # We will use regex to find void propagateValues(float* values) override { ... }
    # Since regex for nested braces is hard, we can just remove specific lines or do text replacement.
    
    # Actually, in MathExpressions.h, it's easy:
    content = re.sub(r'virtual void propagateValues\(float\* values\) = 0;\s*', '', content)
    content = re.sub(r'void propagateValues\(float\* values\) override\s*\{[^}]*\}\s*', '', content)
    # Some overrides don't have bodies inline in Primitives.h ? No, they all do, except OneFloatOperation etc.
    content = re.sub(r'void propagateValues\(float\* values\) override\s*\{\s*valueA->propagateValues\(values\);\s*\}\s*', '', content)
    content = re.sub(r'void propagateValues\(float\* values\) override\s*\{\s*valueA->propagateValues\(values\);\s*valueB->propagateValues\(values\);\s*\}\s*', '', content)
    content = re.sub(r'void propagateValues\(float\* values\) override\s*\{\s*valueA->propagateValues\(values\);\s*valueB->propagateValues\(values\);\s*valueC->propagateValues\(values\);\s*\}\s*', '', content)
    
    # Let's just remove any propagateValues that span multiple lines using a more robust approach:
    # We can match `void propagateValues(float* values) override` and then brace matching.
    
    def remove_propagate(text):
        out = []
        lines = text.split('\n')
        i = 0
        while i < len(lines):
            if "propagateValues(float* values)" in lines[i]:
                # Skip until we find the closing brace or if it's virtual = 0
                if "= 0;" in lines[i]:
                    i += 1
                    continue
                # If there is a '{' in this line or next lines
                brace_count = 0
                started = False
                while i < len(lines):
                    if '{' in lines[i]:
                        brace_count += lines[i].count('{')
                        started = True
                    if '}' in lines[i]:
                        brace_count -= lines[i].count('}')
                    
                    if started and brace_count <= 0:
                        i += 1
                        break
                    i += 1
                continue
            
            # Also remove calls to propagateValues:
            if "propagateValues" in lines[i]:
                i += 1
                continue
                
            out.append(lines[i])
            i += 1
        return '\n'.join(out)

    content = remove_propagate(content)

    # 2. Change float getValue() const to float getValue(const float* parameters) const
    content = content.replace("virtual float getValue() const = 0;", "virtual float getValue(const float* parameters) const = 0;")
    content = content.replace("float getValue() const override", "float getValue(const float* parameters) const override")
    
    # 3. Change valueA->getValue() to valueA->getValue(parameters)
    content = content.replace("->getValue()", "->getValue(parameters)")
    
    # 4. In FloatVariable, change return *m_value; to return parameters[m_offset];
    # Wait, in FloatVariable we removed m_value.
    content = re.sub(r'float\* m_value = nullptr;', '', content)
    content = content.replace('return *m_value;', 'return parameters[m_offset];')
    
    with open(filepath, "w") as f:
        f.write(content)
        
print("Refactoring done.")
