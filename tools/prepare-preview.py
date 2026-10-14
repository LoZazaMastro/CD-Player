from pathlib import Path
import json, shutil, urllib.request
root=Path(__file__).resolve().parents[1]
fixtures=root/'tests/fixtures'; build=root/'build'; build.mkdir(exist_ok=True)
research=build/'http-research';research.mkdir(exist_ok=True)
for name in ['album-info-release.json','album-info-group.json','album-wikidata.json','album-wikipedia.json','current-disc.musicbrainz.json']:
    shutil.copy2(fixtures/name,research/name)
shutil.copy2(fixtures/'reference-disc.musicbrainz.json',build/'apple-disc.musicbrainz.json')
body=json.loads((fixtures/'reference-disc.musicbrainz.json').read_text())
toc=[{'number':i+1,'lba':offset-150} for i,offset in enumerate(body['offsets'])]
toc.append({'number':170,'lba':body['sectors']-150})
(build/'apple-disc.json').write_text(json.dumps({'disc_id':body['id'],'toc':toc}),encoding='utf-8')
reference_release=body['releases'][0]['id']
for release,path in [(reference_release,build/'test-cover.jpg'),('e9e2eaba-acfd-43ad-97c9-0817d94d4a36',build/'preview-blunderbuss.jpg')]:
    if not path.exists():
        request=urllib.request.Request('https://coverartarchive.org/release/'+release+'/front-500',headers={'User-Agent':'CD-Player-host-preview/1.0'})
        with urllib.request.urlopen(request,timeout=30) as response:path.write_bytes(response.read())
print('Public metadata fixtures prepared; artwork cached only under ignored build/.')
