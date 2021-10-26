# ObjCDirectFinder

Clang provide direct attribute for us to write code like below:
```
@property (nonatomic, assign, direct) BOOL isLaunchFinished;
- (BOOL)isLaunching __attribute__((objc_direct));
```
It reduce instructions and skip process of messaging in runtime. So I wrote a plugin to analyze which property/method can be mark as direct, and change about 25700 propertys/methods to direct property/method. **It reduce 3.11 MB size in executale file in my case.**
```
85823088  objc_direct
89084400  original
```

This clang plugin was built and tested in ARM env with **llvm-project branch release/13.x.**

## USAGE

Grab the code and CMakeList file in source directory, and build them as ordinary clang plugin. **Notice that plugin require a  hardcode directory path to the store analyze results **.

```c++
string path = string(<#give me a directory path to store results#>) ...
```

After build your product, a buntch list json file should be listed in directory you provide. Then use merge.py script to merge result:

```shell
python3 merge.py -i path_you_just_provide -o output_file_path
```

Now you get a list that can be mark as directable property/meth:

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