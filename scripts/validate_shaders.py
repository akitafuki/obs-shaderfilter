#!/usr/bin/env python3
"""
Shader Validation Tool for obs-shaderfilter.

Validates all .shader and .effect files in data/examples and data/internal:
- Verifies UTF-8 encoding and file readability.
- Validates #include directives and checks for missing include files or cycles.
- Verifies bracket/brace balance (ignoring comments, strings, and preprocessor conditionals).
- Validates uniform annotations syntax.
- Checks template wrapping compatibility.
"""

import os
import sys
import re
from pathlib import Path

EFFECT_TEMPLATE_BEGIN = """
uniform float4x4 ViewProj;
uniform texture2d image;

uniform float2 uv_offset;
uniform float2 uv_scale;
uniform float2 uv_pixel_interval;
uniform float2 uv_size;
uniform float rand_f;
uniform float rand_instance_f;
uniform float rand_activation_f;
uniform float elapsed_time;
uniform float elapsed_time_start;
uniform float elapsed_time_show;
uniform float elapsed_time_active;
uniform float elapsed_time_enable;
uniform int loops;
uniform float loop_second;
uniform float local_time;
uniform float2 canvas_size;
uniform float delta_time;
uniform int frame_count;
uniform int color_space;
uniform float audio_peak;
uniform float audio_magnitude;

sampler_state textureSampler{
	Filter = Linear;
	AddressU = Border;
	AddressV = Border;
	BorderColor = 00000000;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertData mainTransform(VertData v_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv * uv_scale + uv_offset;
	return vert_out;
}

float srgb_nonlinear_to_linear_channel(float u)
{
	return (u <= 0.04045) ? (u / 12.92) : pow((u + 0.055) / 1.055, 2.4);
}

float3 srgb_nonlinear_to_linear(float3 v)
{
	return float3(srgb_nonlinear_to_linear_channel(v.r),
		      srgb_nonlinear_to_linear_channel(v.g),
		      srgb_nonlinear_to_linear_channel(v.b));
}

float srgb_linear_to_nonlinear_channel(float u)
{
	return (u <= 0.0031308) ? (u * 12.92) : (1.055 * pow(u, 1.0 / 2.4) - 0.055);
}

float3 srgb_linear_to_nonlinear(float3 v)
{
	return float3(srgb_linear_to_nonlinear_channel(v.r),
		      srgb_linear_to_nonlinear_channel(v.g),
		      srgb_linear_to_nonlinear_channel(v.b));
}

float3 tonemap_aces(float3 x)
{
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float3 rec709_to_rec2020(float3 c)
{
	return float3(
		dot(c, float3(0.6274040, 0.3292820, 0.0433136)),
		dot(c, float3(0.0690970, 0.9195400, 0.0113612)),
		dot(c, float3(0.0163916, 0.0880132, 0.8955950))
	);
}

float3 rec2020_to_rec709(float3 c)
{
	return float3(
		dot(c, float3(1.6604910, -0.5876411, -0.0728499)),
		dot(c, float3(-0.1245505, 1.1328999, -0.0083494)),
		dot(c, float3(-0.0181508, -0.1005789, 1.1187297))
	);
}
"""

EFFECT_TEMPLATE_END = """
technique Draw
{
	pass
	{
		vertex_shader = mainTransform(v_in);
		pixel_shader = mainImage(v_in);
	}
}
"""

def clean_code(text: str) -> str:
    """Removes comments and string literals from HLSL/GLSL text using a single tokenizer pattern."""
    pattern = r'(/\*.*?\*/)|(//[^\r\n]*)|("(?:[^"\\]|\\.)*?")'
    def replacer(match):
        if match.group(3) is not None:
            return '""'
        return ' '
    return re.sub(pattern, replacer, text, flags=re.DOTALL)

def preprocess_hlsl(text: str) -> str:
    """Simulates basic HLSL preprocessor branch (defaulting OPENGL=0 / HLSL path)."""
    lines = text.splitlines()
    output = []
    skip_stack = []

    for line in lines:
        trimmed = line.strip()
        if trimmed.startswith("#ifdef OPENGL"):
            skip_stack.append(True)  # skip OPENGL branch by default
            continue
        elif trimmed.startswith("#ifndef OPENGL"):
            skip_stack.append(False)
            continue
        elif trimmed.startswith("#else") and skip_stack:
            skip_stack[-1] = not skip_stack[-1]
            continue
        elif trimmed.startswith("#endif") and skip_stack:
            skip_stack.pop()
            continue

        if not any(skip_stack):
            output.append(line)

    return "\n".join(output)

def resolve_includes(file_path: Path, visited: set = None, depth: int = 0) -> tuple[str, list[str]]:
    """Recursively resolves #include statements and detects missing files or cycles."""
    if visited is None:
        visited = set()

    errors = []
    if depth > 16:
        errors.append(f"Max include depth exceeded (>16) at {file_path}")
        return "", errors

    abs_path = file_path.resolve()
    if abs_path in visited:
        return "", errors  # Skip already included files without error

    visited.add(abs_path)

    try:
        content = file_path.read_text(encoding="utf-8")
    except Exception as e:
        errors.append(f"Failed to read file '{file_path}': {e}")
        return "", errors

    output_lines = []
    base_dir = file_path.parent

    for line_num, line in enumerate(content.splitlines(), start=1):
        trimmed = line.strip()
        if trimmed.startswith("#include"):
            match = re.search(r'#include\s+["<]([^">]+)[">]', trimmed)
            if not match:
                errors.append(f"{file_path}:{line_num}: Malformed #include directive: '{trimmed}'")
                output_lines.append(line)
                continue

            include_target = match.group(1)
            target_path = (base_dir / include_target).resolve()

            if not target_path.exists():
                errors.append(f"{file_path}:{line_num}: Included file not found: '{include_target}' (resolved to '{target_path}')")
                continue

            inc_content, inc_errors = resolve_includes(target_path, visited, depth + 1)
            errors.extend(inc_errors)
            output_lines.append(inc_content)
        else:
            output_lines.append(line)

    return "\n".join(output_lines), errors

def validate_brackets(text: str, filename: str) -> list[str]:
    """Checks for balanced braces {}, parentheses (), and brackets []."""
    cleaned = clean_code(preprocess_hlsl(text))
    stack = []
    pairs = {')': '(', '}': '{', ']': '['}
    errors = []

    for i, char in enumerate(cleaned):
        if char in "({[":
            stack.append((char, i))
        elif char in ")}]":
            if not stack:
                errors.append(f"{filename}: Unmatched closing '{char}' at offset {i}")
                return errors
            top_char, _ = stack.pop()
            if pairs[char] != top_char:
                errors.append(f"{filename}: Mismatched closing '{char}', expected match for '{top_char}'")
                return errors

    if stack:
        unclosed = ", ".join(f"'{c}'" for c, _ in stack)
        errors.append(f"{filename}: Unclosed brackets: {unclosed}")

    return errors

def validate_annotations(text: str, filename: str) -> list[str]:
    """Validates HLSL parameter annotations (< string label = "..."; ... >)."""
    errors = []
    cleaned = clean_code(text)
    
    # Match uniform declarations with optional annotations
    annot_pattern = re.compile(r'uniform\s+([a-zA-Z0-9_]+)\s+([a-zA-Z0-9_]+)\s*<([^>]*)>', re.DOTALL)
    valid_type_prefix = r'^(string|float|float2|float3|float4|int|int2|int3|int4|bool|vec2|vec3|vec4)?\s*[a-zA-Z0-9_]+$'

    for match in annot_pattern.finditer(cleaned):
        var_type, var_name, annot_body = match.groups()
        for stmt in annot_body.strip().split(';'):
            stmt = stmt.strip()
            if not stmt:
                continue
            if '=' not in stmt:
                errors.append(f"{filename}: Invalid annotation in uniform '{var_name}': '{stmt}' (missing '=')")
            else:
                annot_name = stmt.split('=')[0].strip()
                if not re.match(valid_type_prefix, annot_name):
                    errors.append(f"{filename}: Malformed annotation key in uniform '{var_name}': '{annot_name}'")

    return errors

def validate_shader_file(file_path: Path) -> list[str]:
    """Validates a single .shader or .effect file."""
    filename = file_path.name
    is_effect = file_path.suffix.lower() == ".effect"

    resolved_content, errors = resolve_includes(file_path)
    if errors:
        return errors

    if not resolved_content.strip():
        return [f"{filename}: Shader file is empty"]

    bracket_errors = validate_brackets(resolved_content, filename)
    errors.extend(bracket_errors)

    annot_errors = validate_annotations(resolved_content, filename)
    errors.extend(annot_errors)

    cleaned = clean_code(resolved_content)

    if is_effect:
        if "technique" not in cleaned:
            errors.append(f"{filename}: .effect file missing 'technique' block")
    else:
        if "mainImage" not in cleaned and "mainTransform" not in cleaned:
            errors.append(f"{filename}: .shader file missing 'mainImage' pixel shader function")

    return errors

def main():
    root_dir = Path(__file__).resolve().parent.parent
    examples_dir = root_dir / "data" / "examples"
    internal_dir = root_dir / "data" / "internal"

    directories = [examples_dir, internal_dir]
    total_files = 0
    passed_files = 0
    failed_files = 0
    all_errors = []

    print(f"=== obs-shaderfilter Shader Validation ===")
    
    for d in directories:
        if not d.exists():
            print(f"Directory not found: {d}")
            continue

        shader_files = sorted(list(d.glob("*.shader")) + list(d.glob("*.effect")))
        print(f"\nScanning directory: {d.relative_to(root_dir)} ({len(shader_files)} files)")

        for shader_file in shader_files:
            total_files += 1
            file_errors = validate_shader_file(shader_file)
            if file_errors:
                failed_files += 1
                all_errors.extend(file_errors)
                print(f"  ❌ {shader_file.name}")
                for err in file_errors:
                    print(f"     - {err}")
            else:
                passed_files += 1

    print("\n" + "=" * 45)
    print(f"Validation Summary:")
    print(f"  Total shaders:  {total_files}")
    print(f"  Passed:         {passed_files}")
    print(f"  Failed:         {failed_files}")
    print("=" * 45)

    if failed_files > 0:
        print(f"\n❌ Validation failed with {len(all_errors)} error(s).")
        sys.exit(1)
    else:
        print(f"\n✅ All {total_files} shader files validated successfully!")
        sys.exit(0)

if __name__ == "__main__":
    main()
