
# Clear and recreate dist folder
Set-ExecutionPolicy Bypass -Scope Process
Write-Host "Deleting dist/."
rmdir "./dist/*" -Recurse -Force
sleep 1

# Extract contents of wheels folder to it
Get-ChildItem -Path "./wheels" -Filter *.zip | ForEach-Object {
    Expand-Archive -LiteralPath $_.FullName -DestinationPath ./dist/$_
}
sleep 2

# Upload contents of dist to pypi
py -m twine check (Get-ChildItem dist -Recurse -Filter *.whl).FullName
sleep 1

if ((Read-Host -Prompt "Confirm that all wheels passed [Y/n]").ToLower() -eq "y") {
    # py -m twine upload (Get-ChildItem dist -Recurse -Filter *.whl).FullName
    Read-Host -Prompt "Upload complete."
} else {
    Write-Host "Upload cancelled."
}

