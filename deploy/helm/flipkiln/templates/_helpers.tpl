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

{{- define "flipkiln.apiEnv" -}}
-   name: DATABASE_URL
    valueFrom:
        secretKeyRef:
            name: {{ .secret | quote }}
            key: DATABASE_URL
{{- if .mode }}
-   name: FLIPKILN_API_MODE
    value: {{ .mode | quote }}
{{- end }}
-   name: FLIPKILN_DEMO
    value: {{ .root.Values.config.demo | quote }}
-   name: FLIPKILN_COOKIE_SECURE
    value: {{ .root.Values.config.secureCookie | quote }}
{{- if .migration }}
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
{{- end }}
{{- end -}}
