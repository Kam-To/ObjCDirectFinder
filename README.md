# ObjCDirectFinder

Clang had provided `objc_direct` attribute for us to write this:
```
@property (nonatomic, assign, direct) BOOL isLaunchFinished;
- (BOOL)isLaunching __attribute__((objc_direct));
```
It reduce code size and skip the process of Objective-C messaging. So I wrote a plugin to find out property/method as much as possible that can be mark as direct, and changed about 25700 propertys/methods to direct access. **It reduce 3.11 MB size in executale file in my case.**
```
85823088  objc_direct
89084400  original
```

This clang plugin was built and tested under ARM env on **llvm-project  release/13.x branch.**

## USAGE

Grab the code and CMakeList file in source directory, and build them as ordinary clang plugin. **Notice that plugin require a hardcode directory path to the store analyze results**

```c++
string path = string(<#give me a directory path to store results#>) ...
```

After building process, a bunch of json file should be listed in directory you provided. Then use merge.py script to merge results:

```shell
python3 merge.py -i path_you_just_provide -o output_file_path
```

Now you get a json list that property/meth can be marked as directable:

```json
{   "/Users/kam/Documents/XX/PhotoPicker/BullShitPublishPhotoPicker/BullShitPublishTemplatePicker/GLBullShitPublishTemplatePickerViewController.m:30:41": {
        "isPropertyAccessor": true,
        "loc": "/Users/kam/Documents/XX/PhotoPicker/BullShitPublishPhotoPicker/BullShitPublishTemplatePicker/GLBullShitPublishTemplatePickerViewController.m:30:41",
        "name": "-[GLBullShitPublishTemplatePickerViewController() tagId]",
        "sel": "tagId"
    },
    "/Users/kam/Documents/XX/PersonalCenter/Edit/Component/GLPerfectRoleInfoGameComponent.h:14:1": {
        "isPropertyAccessor": false,
        "loc": "/Users/kam/Documents/XX/PersonalCenter/Edit/Component/GLPerfectRoleInfoGameComponent.h:14:1",
        "name": "-[GLPerfectRoleInfoGameComponent configDataWithIconUrl:title:]",
        "sel": "configDataWithIconUrl:title:"
    },
    ...
}
```

## LICENSE

MIT LICENSE.