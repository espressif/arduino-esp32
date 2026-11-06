#!/usr/bin/env python3
"""
OTA Update Signing Tool for ESP32 Arduino

This script signs firmware binaries for secure OTA updates.
It supports both RSA and ECDSA signing schemes with various hash algorithms.

Usage:
    python bin_signing.py --bin firmware.bin --key private_key.pem --out firmware_signed.bin
    python bin_signing.py --generate-key rsa-2048 --out private_key.pem
    python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
"""

import argparse
import sys
import os
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ec, padding
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.serialization import load_pem_private_key, load_pem_public_key


def generate_rsa_key(key_size, output_file):
    """Generate an RSA private key"""
    print(f"Generating RSA-{key_size} private key...")
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=key_size, backend=default_backend())

    pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption(),
    )

    with open(output_file, "wb") as f:
        f.write(pem)

    print(f"Private key saved to: {output_file}")
    print("\nIMPORTANT: Keep this private key secure!")
    print("Extract the public key with: python bin_signing.py --extract-pubkey", output_file)


def generate_ecdsa_key(curve_name, output_file):
    """Generate an ECDSA private key"""
    curves = {
        "p256": ec.SECP256R1(),
        "p384": ec.SECP384R1(),
    }

    if curve_name not in curves:
        print(f"Error: Unsupported curve. Supported curves: {', '.join(curves.keys())}")
        sys.exit(1)

    print(f"Generating ECDSA-{curve_name.upper()} private key...")
    private_key = ec.generate_private_key(curves[curve_name], backend=default_backend())

    pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.TraditionalOpenSSL,
        encryption_algorithm=serialization.NoEncryption(),
    )

    with open(output_file, "wb") as f:
        f.write(pem)

    print(f"Private key saved to: {output_file}")
    print("\nIMPORTANT: Keep this private key secure!")
    print("Extract the public key with: python bin_signing.py --extract-pubkey", output_file)


def extract_public_key(private_key_file, output_file):
    """Extract public key from private key"""
    print(f"Extracting public key from {private_key_file}...")

    with open(private_key_file, "rb") as f:
        private_key = load_pem_private_key(f.read(), password=None, backend=default_backend())

    public_key = private_key.public_key()

    pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM, format=serialization.PublicFormat.SubjectPublicKeyInfo
    )

    with open(output_file, "wb") as f:
        f.write(pem)

    print(f"Public key saved to: {output_file}")

    # Also generate a C header file for embedding in Arduino sketch
    header_file = os.path.splitext(output_file)[0] + ".h"
    with open(header_file, "w") as f:
        f.write("// Public key for OTA signature verification\n")
        f.write("// Include this in your Arduino sketch\n\n")
        f.write("const uint8_t PUBLIC_KEY[] PROGMEM = {\n")

        # Add null terminator for mbedtls PEM parser
        pem_bytes = pem + b"\x00"
        for i in range(0, len(pem_bytes), 16):
            chunk = pem_bytes[i : i + 16]
            hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"  {hex_str},\n")

        f.write("};\n")
        f.write(f"const size_t PUBLIC_KEY_LEN = {len(pem_bytes)};\n")

    print(f"C header file saved to: {header_file}")


def sign_binary(binary_file, key_file, output_file, hash_algo="sha256"):
    """Sign a binary file"""
    print(f"Signing {binary_file} with {key_file}...")

    # Read the binary
    with open(binary_file, "rb") as f:
        binary_data = f.read()

    print(f"Binary size: {len(binary_data)} bytes")

    # Load private key
    with open(key_file, "rb") as f:
        private_key = load_pem_private_key(f.read(), password=None, backend=default_backend())

    # Select hash algorithm
    hash_algos = {
        "sha256": hashes.SHA256(),
        "sha384": hashes.SHA384(),
        "sha512": hashes.SHA512(),
    }

    if hash_algo not in hash_algos:
        print(f"Error: Unsupported hash algorithm. Supported: {', '.join(hash_algos.keys())}")
        sys.exit(1)

    hash_obj = hash_algos[hash_algo]

    # Sign the binary
    if isinstance(private_key, rsa.RSAPrivateKey):
        print(f"Using RSA-PSS with {hash_algo.upper()}")
        signature = private_key.sign(
            binary_data, padding.PSS(mgf=padding.MGF1(hash_obj), salt_length=padding.PSS.MAX_LENGTH), hash_obj
        )
        key_type = "RSA"
    elif isinstance(private_key, ec.EllipticCurvePrivateKey):
        print(f"Using ECDSA with {hash_algo.upper()}")
        signature = private_key.sign(binary_data, ec.ECDSA(hash_obj))
        key_type = "ECDSA"
    else:
        print("Error: Unsupported key type")
        sys.exit(1)

    print(f"Signature size: {len(signature)} bytes")

    # Pad signature to max size (512 bytes for RSA-4096)
    max_sig_size = 512
    padded_signature = signature + b"\x00" * (max_sig_size - len(signature))

    # Write signed binary (firmware + signature)
    with open(output_file, "wb") as f:
        f.write(binary_data)
        f.write(padded_signature)

    signed_size = len(binary_data) + len(padded_signature)
    print(f"Signed binary saved to: {output_file}")
    print(f"Signed binary size: {signed_size} bytes (firmware: {len(binary_data)}, signature: {len(padded_signature)})")
    print(f"\nKey type: {key_type}")
    print(f"Hash algorithm: {hash_algo.upper()}")


def verify_signature(binary_file, pubkey_file, hash_algo="sha256"):
    """Verify a signed binary"""
    print(f"Verifying signature of {binary_file} with {pubkey_file}...")

    # Read the signed binary
    with open(binary_file, "rb") as f:
        signed_data = f.read()

    # The signature is the last 512 bytes (padded)
    max_sig_size = 512
    if len(signed_data) < max_sig_size:
        print("Error: File too small to contain signature")
        sys.exit(1)

    binary_data = signed_data[:-max_sig_size]
    signature = signed_data[-max_sig_size:]

    # Load public key
    with open(pubkey_file, "rb") as f:
        public_key = load_pem_public_key(f.read(), backend=default_backend())

    # Select hash algorithm
    hash_algos = {
        "sha256": hashes.SHA256(),
        "sha384": hashes.SHA384(),
        "sha512": hashes.SHA512(),
    }

    if hash_algo not in hash_algos:
        print(f"Error: Unsupported hash algorithm. Supported: {', '.join(hash_algos.keys())}")
        sys.exit(1)

    hash_obj = hash_algos[hash_algo]

    # Remove padding from signature
    signature = signature.rstrip(b"\x00")

    # Verify the signature
    try:
        if isinstance(public_key, rsa.RSAPublicKey):
            print(f"Verifying RSA-PSS signature with {hash_algo.upper()}")
            public_key.verify(
                signature,
                binary_data,
                padding.PSS(mgf=padding.MGF1(hash_obj), salt_length=padding.PSS.MAX_LENGTH),
                hash_obj,
            )
        elif isinstance(public_key, ec.EllipticCurvePublicKey):
            print(f"Verifying ECDSA signature with {hash_algo.upper()}")
            public_key.verify(signature, binary_data, ec.ECDSA(hash_obj))
        else:
            print("Error: Unsupported key type")
            sys.exit(1)

        print("✓ Signature verification SUCCESSFUL!")
        return True
    except Exception as e:
        print(f"✗ Signature verification FAILED: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="OTA Update Signing Tool for ESP32 Arduino",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Generate RSA-2048 key:
    python bin_signing.py --generate-key rsa-2048 --out private_key.pem

  Generate ECDSA-P256 key:
    python bin_signing.py --generate-key ecdsa-p256 --out private_key.pem

  Extract public key:
    python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem

  Sign firmware:
    python bin_signing.py --bin firmware.bin --key private_key.pem --out firmware_signed.bin

  Sign with SHA-384:
    python bin_signing.py --bin firmware.bin --key private_key.pem --out firmware_signed.bin --hash sha384

  Verify signed firmware:
    python bin_signing.py --verify firmware_signed.bin --pubkey public_key.pem
        """,
    )

    parser.add_argument(
        "--generate-key",
        metavar="TYPE",
        help="Generate a new key (rsa-2048, rsa-3072, rsa-4096, ecdsa-p256, ecdsa-p384)",
    )
    parser.add_argument("--extract-pubkey", metavar="PRIVATE_KEY", help="Extract public key from private key")
    parser.add_argument("--bin", metavar="FILE", help="Binary file to sign")
    parser.add_argument("--key", metavar="FILE", help="Private key file (PEM format)")
    parser.add_argument("--pubkey", metavar="FILE", help="Public key file for verification (PEM format)")
    parser.add_argument("--out", metavar="FILE", help="Output file")
    parser.add_argument(
        "--hash", default="sha256", choices=["sha256", "sha384", "sha512"], help="Hash algorithm (default: sha256)"
    )
    parser.add_argument("--verify", metavar="FILE", help="Verify a signed binary")

    args = parser.parse_args()

    if args.generate_key:
        if not args.out:
            print("Error: --out required for key generation")
            sys.exit(1)

        key_type = args.generate_key.lower()
        if key_type.startswith("rsa-"):
            key_size = int(key_type.split("-")[1])
            generate_rsa_key(key_size, args.out)
        elif key_type.startswith("ecdsa-"):
            curve = key_type.split("-")[1]
            generate_ecdsa_key(curve, args.out)
        else:
            print("Error: Invalid key type. Supported: rsa-2048, rsa-3072, rsa-4096, ecdsa-p256, ecdsa-p384")
            sys.exit(1)

    elif args.extract_pubkey:
        if not args.out:
            print("Error: --out required for public key extraction")
            sys.exit(1)
        extract_public_key(args.extract_pubkey, args.out)

    elif args.verify:
        if not args.pubkey:
            print("Error: --pubkey required for verification")
            sys.exit(1)
        verify_signature(args.verify, args.pubkey, args.hash)

    elif args.bin and args.key:
        if not args.out:
            print("Error: --out required for signing")
            sys.exit(1)
        sign_binary(args.bin, args.key, args.out, args.hash)

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == "__main__":
    main()
