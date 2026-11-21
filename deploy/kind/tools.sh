#!/usr/bin/env bash
set -euo pipefail

bin=${1:-"$PWD/.flipkiln-tools"}
mkdir -p "$bin"
case "$(uname -s)" in
    Linux) os=linux ;;
    Darwin) os=darwin ;;
    *)
        echo 'Use Linux or macOS to download the kind tools.' >&2
        exit 1
        ;;
esac
case "$(uname -m)" in
    x86_64) arch=amd64 ;;
    arm64 | aarch64) arch=arm64 ;;
    *)
        echo 'Use amd64 or arm64 to download the kind tools.' >&2
        exit 1
        ;;
esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

download() {
    local url=$1 destination=$2 checksum_url=$3 checksum
    curl -fsSL --retry 3 "$url" -o "$destination"
    checksum=$(curl -fsSL --retry 3 "$checksum_url" | awk 'NR == 1 {print $1}')
    printf '%s  %s\n' "$checksum" "$destination" | shasum -a 256 -c -
}

kind_version=v0.29.0
kubectl_version=v1.33.1
helm_version=v3.18.6
kind_url="https://github.com/kubernetes-sigs/kind/releases/download/$kind_version/kind-$os-$arch"
kubectl_url="https://dl.k8s.io/release/$kubectl_version/bin/$os/$arch/kubectl"
helm_archive="helm-$helm_version-$os-$arch.tar.gz"
helm_url="https://get.helm.sh/$helm_archive"
download "$kind_url" "$work/kind" "$kind_url.sha256sum"
download "$kubectl_url" "$work/kubectl" "$kubectl_url.sha256"
download "$helm_url" "$work/$helm_archive" "$helm_url.sha256sum"
tar -xzf "$work/$helm_archive" -C "$work" "$os-$arch/helm"
install -m 0755 "$work/kind" "$work/kubectl" "$work/$os-$arch/helm" "$bin"
printf 'Tools are in %s\n' "$bin"
