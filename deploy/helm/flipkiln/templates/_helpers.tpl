{{- define "flipkiln.labels" -}}
app.kubernetes.io/name: flipkiln
app.kubernetes.io/instance: {{ .Release.Name | quote }}
{{- end -}}

{{- define "flipkiln.containerSecurity" -}}
allowPrivilegeEscalation: false
readOnlyRootFilesystem: true
capabilities:
    drop: [ALL]
{{- end -}}

{{- define "flipkiln.podSecurity" -}}
runAsNonRoot: true
seccompProfile:
    type: RuntimeDefault
{{- end -}}

{{- define "flipkiln.image" -}}
{{- $repository := default .fallback.repository .image.repository -}}
{{- $tag := default .fallback.tag .image.tag -}}
{{- printf "%s:%s" $repository $tag -}}
{{- end -}}

{{- define "flipkiln.storageName" -}}
{{- printf "%s-storage-api" .Release.Name | trunc 63 | trimSuffix "-" -}}
{{- end -}}

{{- define "flipkiln.apiEnv" -}}
{{- $modeKey := ternary "FLIPKILN_STORAGE_CONTROL_KEY" "FLIPKILN_STORAGE_RUNTIME_KEY" (eq .mode "control") -}}
-   name: FLIPKILN_API_MODE
    value: {{ .mode | quote }}
{{- if eq .mode "storage" }}
{{- $databaseSecret := required "secrets.existingSecret is required" .root.Values.secrets.existingSecret }}
{{- $readerSecret := required "runtime.existingSecret is required" .root.Values.runtime.existingSecret }}
-   name: DATABASE_URL
    valueFrom:
        secretKeyRef:
            name: {{ $databaseSecret | quote }}
            key: DATABASE_URL
-   name: FLIPKILN_STORAGE_READER_DATABASE_URL
    valueFrom:
        secretKeyRef:
            name: {{ $readerSecret | quote }}
            key: DATABASE_URL
-   name: FLIPKILN_COOKIE_SECURE
    value: {{ .root.Values.config.secureCookie | quote }}
{{- range $key := list "FLIPKILN_STORAGE_CONTROL_KEY" "FLIPKILN_STORAGE_RUNTIME_KEY" }}
-   name: {{ $key }}
    valueFrom:
        secretKeyRef:
            name: {{ $.root.Values.storage.existingSecret | quote }}
            key: {{ $key }}
{{- end }}
{{- else }}
-   name: FLIPKILN_STORAGE_URL
    value: http://{{ include "flipkiln.storageName" .root }}:8080
-   name: FLIPKILN_STORAGE_KEY
    valueFrom:
        secretKeyRef:
            name: {{ .root.Values.storage.existingSecret | quote }}
            key: {{ $modeKey }}
{{- end }}
{{- end -}}

{{- define "flipkiln.migrationEnv" -}}
-   name: DATABASE_URL
    valueFrom:
        secretKeyRef:
            name: {{ .secret | quote }}
            key: DATABASE_URL
-   name: FLIPKILN_DEMO
    value: {{ .root.Values.config.demo | quote }}
-   name: FLIPKILN_RUNTIME_ROLE
    value: {{ required "migrations.runtimeRole is required" .root.Values.migrations.runtimeRole | quote }}
-   name: FLIPKILN_ADMIN_PASSWORD
    valueFrom:
        secretKeyRef:
            name: {{ .secret | quote }}
            key: FLIPKILN_ADMIN_PASSWORD
{{- if .root.Values.config.demo }}
-   name: FLIPKILN_DEMO_TOKEN
    valueFrom:
        secretKeyRef:
            name: {{ .secret | quote }}
            key: FLIPKILN_DEMO_TOKEN
            optional: true
{{- end }}
{{- end -}}
